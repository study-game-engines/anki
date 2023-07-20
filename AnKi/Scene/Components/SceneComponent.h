// Copyright (C) 2009-2023, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Scene/Common.h>
#include <AnKi/Util/Functions.h>
#include <AnKi/Util/BitMask.h>
#include <AnKi/Util/Enum.h>

namespace anki {

/// @addtogroup scene
/// @{

/// @memberof SceneComponent
enum class SceneComponentType : U8
{
#define ANKI_DEFINE_SCENE_COMPONENT(name, weight) k##name,
#include <AnKi/Scene/Components/SceneComponentClasses.def.h>

	kCount,
	kFirst = 0
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(SceneComponentType)

/// @memberof SceneComponent
enum class SceneComponentTypeMask : U32
{
	kNone = 0,

#define ANKI_DEFINE_SCENE_COMPONENT(name, weight) k##name = 1 << U32(SceneComponentType::k##name),
#include <AnKi/Scene/Components/SceneComponentClasses.def.h>
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(SceneComponentTypeMask)

class SceneComponentType2
{
public:
#define ANKI_DEFINE_SCENE_COMPONENT(name, weight) static constexpr SceneComponentType k##name##Component = SceneComponentType::k##name;
#include <AnKi/Scene/Components/SceneComponentClasses.def.h>
};

#define ANKI_SCENE_COMPONENT(className) \
public: \
	static constexpr SceneComponentType kClassType = SceneComponentType2::k##className; \
\
private:

/// Passed to SceneComponent::update.
/// @memberof SceneComponent
class SceneComponentUpdateInfo
{
public:
	SceneNode* m_node = nullptr;
	const Second m_previousTime;
	const Second m_currentTime;
	const Second m_dt;
	StackMemoryPool* m_framePool = nullptr;

	SceneComponentUpdateInfo(Second prevTime, Second crntTime)
		: m_previousTime(prevTime)
		, m_currentTime(crntTime)
		, m_dt(crntTime - prevTime)
	{
	}
};

/// Scene node component.
/// @note Doesn't have C++ virtuals to keep it compact.
class SceneComponent
{
public:
	/// Construct the scene component.
	SceneComponent([[maybe_unused]] SceneNode* node, SceneComponentType type)
		: m_type(type)
	{
	}

	SceneComponentType getType() const
	{
		return m_type;
	}

	Timestamp getTimestamp() const
	{
		return m_timestamp;
	}

	ANKI_INTERNAL U32 getArrayIndex() const
	{
		ANKI_ASSERT(m_arrayIdx != kMaxU32);
		return m_arrayIdx;
	}

	ANKI_INTERNAL void setArrayIndex(U32 idx)
	{
		m_arrayIdx = idx;
	}

	ANKI_INTERNAL virtual void onDestroy([[maybe_unused]] SceneNode& node)
	{
	}

	ANKI_INTERNAL virtual Error update(SceneComponentUpdateInfo& info, Bool& updated) = 0;

	ANKI_INTERNAL virtual void onOtherComponentRemovedOrAdded([[maybe_unused]] SceneComponent* other, [[maybe_unused]] Bool added)
	{
	}

	/// Don't call it directly.
	ANKI_INTERNAL void setTimestamp(Timestamp timestamp)
	{
		ANKI_ASSERT(timestamp > 0);
		ANKI_ASSERT(timestamp >= m_timestamp);
		m_timestamp = timestamp;
	}

	static constexpr F32 getUpdateOrderWeight(SceneComponentType type)
	{
		return m_updateOrderWeights[type];
	}

private:
	Timestamp m_timestamp = 1; ///< Indicates when an update happened
	U32 m_arrayIdx = kMaxU32;
	SceneComponentType m_type; ///< Cache the type ID.

	static constexpr Array<F32, U32(SceneComponentType::kCount)> m_updateOrderWeights = {
#define ANKI_DEFINE_SCENE_COMPONENT(name, weight) weight
#define ANKI_SCENE_COMPONENT_SEPARATOR ,
#include <AnKi/Scene/Components/SceneComponentClasses.def.h>
	};
};
/// @}

} // end namespace anki
