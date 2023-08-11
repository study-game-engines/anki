// Copyright (C) 2009-2023, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Scene/SceneGraph.h>
#include <AnKi/Scene/Octree.h>
#include <AnKi/Scene/RenderStateBucket.h>
#include <AnKi/Physics/PhysicsWorld.h>
#include <AnKi/Resource/ResourceManager.h>
#include <AnKi/Renderer/MainRenderer.h>
#include <AnKi/Core/CVarSet.h>
#include <AnKi/Core/StatsSet.h>
#include <AnKi/Util/ThreadHive.h>
#include <AnKi/Util/Tracer.h>
#include <AnKi/Util/HighRezTimer.h>

#include <AnKi/Scene/Components/BodyComponent.h>
#include <AnKi/Scene/Components/CameraComponent.h>
#include <AnKi/Scene/Components/DecalComponent.h>
#include <AnKi/Scene/Components/FogDensityComponent.h>
#include <AnKi/Scene/Components/GlobalIlluminationProbeComponent.h>
#include <AnKi/Scene/Components/JointComponent.h>
#include <AnKi/Scene/Components/LensFlareComponent.h>
#include <AnKi/Scene/Components/LightComponent.h>
#include <AnKi/Scene/Components/ModelComponent.h>
#include <AnKi/Scene/Components/MoveComponent.h>
#include <AnKi/Scene/Components/ParticleEmitterComponent.h>
#include <AnKi/Scene/Components/PlayerControllerComponent.h>
#include <AnKi/Scene/Components/ReflectionProbeComponent.h>
#include <AnKi/Scene/Components/ScriptComponent.h>
#include <AnKi/Scene/Components/SkinComponent.h>
#include <AnKi/Scene/Components/SkyboxComponent.h>
#include <AnKi/Scene/Components/TriggerComponent.h>
#include <AnKi/Scene/Components/UiComponent.h>

namespace anki {

static StatCounter g_sceneUpdateTime(StatCategory::kTime, "All scene update", StatFlag::kMilisecond | StatFlag::kShowAverage);
static StatCounter g_sceneVisibilityTime(StatCategory::kTime, "Scene visibility", StatFlag::kMilisecond | StatFlag::kShowAverage);
static StatCounter g_scenePhysicsTime(StatCategory::kTime, "Physics", StatFlag::kMilisecond | StatFlag::kShowAverage);

static NumericCVar<U32> g_octreeMaxDepthCVar(CVarSubsystem::kScene, "OctreeMaxDepth", 5, 2, 10, "The max depth of the octree");

NumericCVar<F32> g_probeEffectiveDistanceCVar(CVarSubsystem::kScene, "ProbeEffectiveDistance", 256.0f, 1.0f, kMaxF32,
											  "How far various probes can render");
NumericCVar<F32> g_probeShadowEffectiveDistanceCVar(CVarSubsystem::kScene, "ProbeShadowEffectiveDistance", 32.0f, 1.0f, kMaxF32,
													"How far to render shadows for the various probes");

// Gpu scene arrays
static NumericCVar<U32> g_minGpuSceneTransformsCVar(CVarSubsystem::kScene, "MinGpuSceneTransforms", 2 * 10 * 1024, 8, 100 * 1024,
													"The min number of transforms stored in the GPU scene");
static NumericCVar<U32> g_minGpuSceneMeshesCVar(CVarSubsystem::kScene, "MinGpuSceneMeshes", 8 * 1024, 8, 100 * 1024,
												"The min number of meshes stored in the GPU scene");
static NumericCVar<U32> g_minGpuSceneParticleEmittersCVar(CVarSubsystem::kScene, "MinGpuSceneParticleEmitters", 1 * 1024, 8, 100 * 1024,
														  "The min number of particle emitters stored in the GPU scene");
static NumericCVar<U32> g_minGpuSceneLightsCVar(CVarSubsystem::kScene, "MinGpuSceneLights", 2 * 1024, 8, 100 * 1024,
												"The min number of lights stored in the GPU scene");
static NumericCVar<U32> g_minGpuSceneReflectionProbesCVar(CVarSubsystem::kScene, "MinGpuSceneReflectionProbes", 128, 8, 100 * 1024,
														  "The min number of reflection probes stored in the GPU scene");
static NumericCVar<U32> g_minGpuSceneGlobalIlluminationProbesCVar(CVarSubsystem::kScene, "MinGpuSceneGlobalIlluminationProbes", 128, 8, 100 * 1024,
																  "The min number of GI probes stored in the GPU scene");
static NumericCVar<U32> g_minGpuSceneDecalsCVar(CVarSubsystem::kScene, "MinGpuSceneDecals", 2 * 1024, 8, 100 * 1024,
												"The min number of decals stored in the GPU scene");
static NumericCVar<U32> g_minGpuSceneFogDensityVolumesCVar(CVarSubsystem::kScene, "MinGpuSceneFogDensityVolumes", 512, 8, 100 * 1024,
														   "The min number fog density volumes stored in the GPU scene");
static NumericCVar<U32> g_minGpuSceneRenderablesCVar(CVarSubsystem::kScene, "MinGpuSceneRenderables", 10 * 1024, 8, 100 * 1024,
													 "The min number of renderables stored in the GPU scene");

constexpr U32 kUpdateNodeBatchSize = 10;

class SceneGraph::UpdateSceneNodesCtx
{
public:
	SceneGraph* m_scene = nullptr;

	IntrusiveList<SceneNode>::Iterator m_crntNode;
	SpinLock m_crntNodeLock;

	Second m_prevUpdateTime;
	Second m_crntTime;
};

SceneGraph::SceneGraph()
{
}

SceneGraph::~SceneGraph()
{
	[[maybe_unused]] const Error err = iterateSceneNodes([&](SceneNode& s) -> Error {
		s.setMarkedForDeletion();
		return Error::kNone;
	});

	deleteNodesMarkedForDeletion();

	if(m_octree)
	{
		deleteInstance(SceneMemoryPool::getSingleton(), m_octree);
	}

#define ANKI_CAT_TYPE(arrayName, gpuSceneType, id, cvarName) GpuSceneArrays::arrayName::freeSingleton();
#include <AnKi/Scene/GpuSceneArrays.def.h>

	RenderStateBucketContainer::freeSingleton();
}

Error SceneGraph::init(AllocAlignedCallback allocCallback, void* allocCallbackData)
{
	SceneMemoryPool::allocateSingleton(allocCallback, allocCallbackData);

	m_framePool.init(allocCallback, allocCallbackData, 1_MB, 2.0, 0, true, ANKI_SAFE_ALIGNMENT, "SceneGraphFramePool");

	m_octree = newInstance<Octree>(SceneMemoryPool::getSingleton());
	m_octree->init(Vec3(-1000.0f, -200.0f, -1000.0f), Vec3(1000.0f, 200.0f, 1000.0f), g_octreeMaxDepthCVar.get());

	// Init the default main camera
	ANKI_CHECK(newSceneNode<SceneNode>("mainCamera", m_defaultMainCam));
	CameraComponent* camc = m_defaultMainCam->newComponent<CameraComponent>();
	camc->setPerspective(0.1f, 1000.0f, toRad(60.0f), (1080.0f / 1920.0f) * toRad(60.0f));
	m_mainCam = m_defaultMainCam;

#define ANKI_CAT_TYPE(arrayName, gpuSceneType, id, cvarName) GpuSceneArrays::arrayName::allocateSingleton(cvarName.get());
#include <AnKi/Scene/GpuSceneArrays.def.h>

	RenderStateBucketContainer::allocateSingleton();

	return Error::kNone;
}

Error SceneGraph::registerNode(SceneNode* node)
{
	ANKI_ASSERT(node);

	// Add to dict if it has a name
	if(node->getName())
	{
		if(tryFindSceneNode(node->getName()))
		{
			ANKI_SCENE_LOGE("Node with the same name already exists");
			return Error::kUserData;
		}

		m_nodesDict.emplace(node->getName(), node);
	}

	// Add to vector
	m_nodes.pushBack(node);
	++m_nodesCount;

	return Error::kNone;
}

void SceneGraph::unregisterNode(SceneNode* node)
{
	// Remove from the graph
	m_nodes.erase(node);
	--m_nodesCount;

	if(m_mainCam != m_defaultMainCam && m_mainCam == node)
	{
		m_mainCam = m_defaultMainCam;
	}

	// Remove from dict
	if(node->getName())
	{
		auto it = m_nodesDict.find(node->getName());
		ANKI_ASSERT(it != m_nodesDict.getEnd());
		m_nodesDict.erase(it);
	}
}

SceneNode& SceneGraph::findSceneNode(const CString& name)
{
	SceneNode* node = tryFindSceneNode(name);
	ANKI_ASSERT(node);
	return *node;
}

SceneNode* SceneGraph::tryFindSceneNode(const CString& name)
{
	auto it = m_nodesDict.find(name);
	return (it == m_nodesDict.getEnd()) ? nullptr : (*it);
}

void SceneGraph::deleteNodesMarkedForDeletion()
{
	/// Delete all nodes pending deletion. At this point all scene threads
	/// should have finished their tasks
	while(m_objectsMarkedForDeletionCount.load() > 0)
	{
		[[maybe_unused]] Bool found = false;
		auto it = m_nodes.begin();
		auto end = m_nodes.end();
		for(; it != end; ++it)
		{
			SceneNode& node = *it;

			if(node.getMarkedForDeletion())
			{
				// Delete node
				unregisterNode(&node);
				deleteInstance(SceneMemoryPool::getSingleton(), &node);
				m_objectsMarkedForDeletionCount.fetchSub(1);
				found = true;
				break;
			}
		}

		ANKI_ASSERT(found && "Something is wrong with marked for deletion");
	}
}

Error SceneGraph::update(Second prevUpdateTime, Second crntTime)
{
	ANKI_ASSERT(m_mainCam);
	ANKI_TRACE_SCOPED_EVENT(SceneUpdate);

	const Second startUpdateTime = HighRezTimer::getCurrentTime();

	// Reset the framepool
	m_framePool.reset();

	// Delete stuff
	{
		ANKI_TRACE_SCOPED_EVENT(SceneRemoveMarkedForDeletion);
		const Bool fullCleanup = m_objectsMarkedForDeletionCount.load() != 0;
		m_events.deleteEventsMarkedForDeletion(fullCleanup);
		deleteNodesMarkedForDeletion();
	}

	// Update
	{
		ANKI_TRACE_SCOPED_EVENT(ScenePhysics);
		const Second physicsUpdate = HighRezTimer::getCurrentTime();

		PhysicsWorld::getSingleton().update(crntTime - prevUpdateTime);

		g_scenePhysicsTime.set((HighRezTimer::getCurrentTime() - physicsUpdate) * 1000.0);
	}

	{
		ANKI_TRACE_SCOPED_EVENT(SceneNodesUpdate);
		ANKI_CHECK(m_events.updateAllEvents(prevUpdateTime, crntTime));

		// Then the rest
		Array<ThreadHiveTask, ThreadHive::kMaxThreads> tasks;
		UpdateSceneNodesCtx updateCtx;
		updateCtx.m_scene = this;
		updateCtx.m_crntNode = m_nodes.getBegin();
		updateCtx.m_prevUpdateTime = prevUpdateTime;
		updateCtx.m_crntTime = crntTime;

		for(U i = 0; i < CoreThreadHive::getSingleton().getThreadCount(); i++)
		{
			tasks[i] = ANKI_THREAD_HIVE_TASK(
				{
					if(self->m_scene->updateNodes(*self))
					{
						ANKI_SCENE_LOGF("Will not recover");
					}
				},
				&updateCtx, nullptr, nullptr);
		}

		CoreThreadHive::getSingleton().submitTasks(&tasks[0], CoreThreadHive::getSingleton().getThreadCount());
		CoreThreadHive::getSingleton().waitAllTasks();
	}

#define ANKI_CAT_TYPE(arrayName, gpuSceneType, id, cvarName) GpuSceneArrays::arrayName::getSingleton().flush();
#include <AnKi/Scene/GpuSceneArrays.def.h>

	g_sceneUpdateTime.set((HighRezTimer::getCurrentTime() - startUpdateTime) * 1000.0);
	return Error::kNone;
}

void SceneGraph::doVisibilityTests(RenderQueue& rqueue)
{
	const Second startTime = HighRezTimer::getCurrentTime();
	doVisibilityTests(*m_mainCam, *this, rqueue);
	g_sceneVisibilityTime.set((HighRezTimer::getCurrentTime() - startTime) * 1000.0);
}

Error SceneGraph::updateNode(Second prevTime, Second crntTime, SceneNode& node)
{
	ANKI_TRACE_INC_COUNTER(SceneNodeUpdated, 1);

	Error err = Error::kNone;

	// Components update
	SceneComponentUpdateInfo componentUpdateInfo(prevTime, crntTime);
	componentUpdateInfo.m_framePool = &m_framePool;

	Bool atLeastOneComponentUpdated = false;
	node.iterateComponents([&](SceneComponent& comp) {
		if(err)
		{
			return;
		}

		componentUpdateInfo.m_node = &node;
		Bool updated = false;
		err = comp.update(componentUpdateInfo, updated);

		if(updated)
		{
			ANKI_TRACE_INC_COUNTER(SceneComponentUpdated, 1);
			comp.setTimestamp(GlobalFrameIndex::getSingleton().m_value);
			atLeastOneComponentUpdated = true;
		}
	});

	// Update children
	if(!err)
	{
		err = node.visitChildrenMaxDepth(0, [&](SceneNode& child) -> Error {
			return updateNode(prevTime, crntTime, child);
		});
	}

	// Frame update
	if(!err)
	{
		if(atLeastOneComponentUpdated)
		{
			node.setComponentMaxTimestamp(GlobalFrameIndex::getSingleton().m_value);
		}
		else
		{
			// No components or nothing updated, don't change the timestamp
		}

		err = node.frameUpdate(prevTime, crntTime);
	}

	return err;
}

Error SceneGraph::updateNodes(UpdateSceneNodesCtx& ctx)
{
	ANKI_TRACE_SCOPED_EVENT(SceneNodeUpdate);

	IntrusiveList<SceneNode>::ConstIterator end = m_nodes.getEnd();

	Bool quit = false;
	Error err = Error::kNone;
	while(!quit && !err)
	{
		// Fetch a batch of scene nodes that don't have parent
		Array<SceneNode*, kUpdateNodeBatchSize> batch;
		U batchSize = 0;

		{
			LockGuard<SpinLock> lock(ctx.m_crntNodeLock);

			while(1)
			{
				if(batchSize == batch.getSize())
				{
					break;
				}

				if(ctx.m_crntNode == end)
				{
					quit = true;
					break;
				}

				SceneNode& node = *ctx.m_crntNode;
				if(node.getParent() == nullptr)
				{
					batch[batchSize++] = &node;
				}

				++ctx.m_crntNode;
			}
		}

		// Process nodes
		for(U i = 0; i < batchSize && !err; ++i)
		{
			err = updateNode(ctx.m_prevUpdateTime, ctx.m_crntTime, *batch[i]);
		}
	}

	return err;
}

LightComponent* SceneGraph::getDirectionalLight() const
{
	LightComponent* out = (m_dirLights.getSize()) ? m_dirLights[0] : nullptr;
	if(out)
	{
		ANKI_ASSERT(out->getLightComponentType() == LightComponentType::kDirectional);
	}
	return out;
}

} // end namespace anki
