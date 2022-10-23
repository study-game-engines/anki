-- Generated by: C:\Users\godli\src\anki\out\build\x64-Release\Bin\GltfImporter.exe droid.gltf C:/Users/godli/src/anki/Samples/SkeletalAnimation/Assets/ -rpath Assets -texrpath Assets -import-textures 1 -v
local scene = getSceneGraph()
local events = getEventManager()

node = scene:newModelNode("droid.001")
node:getSceneNodeBase():getModelComponent():loadModelResource("Assets/Mesh_Robot.001_514ce62fac09d811.ankimdl")
node:getSceneNodeBase():getSkinComponent():loadSkeletonResource("Assets/Armature.002_9ddcea0a08bd9d11.ankiskel")
trf = Transform.new()
trf:setOrigin(Vec4.new(0.000000, 0.000000, 0.000000, 0))
rot = Mat3x4.new()
rot:setAll(1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000)
trf:setRotation(rot)
trf:setScale(1.000000)
node:getSceneNodeBase():getMoveComponent():setLocalTransform(trf)

node = scene:newGlobalIlluminationProbeNode("Cube")
comp = node:getSceneNodeBase():getGlobalIlluminationProbeComponent()
comp:setBoxVolumeSize(Vec3.new(19.286558, 19.286558, 19.286558))
trf = Transform.new()
trf:setOrigin(Vec4.new(0.000000, 11.057286, 0.000000, 0))
rot = Mat3x4.new()
rot:setAll(1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000)
trf:setRotation(rot)
trf:setScale(1.000000)
node:getSceneNodeBase():getMoveComponent():setLocalTransform(trf)

node = scene:newReflectionProbeNode("Cube.001")
comp = node:getSceneNodeBase():getReflectionProbeComponent()
comp:setBoxVolumeSize(Vec3.new(18.543777, 18.543777, 18.543777))
trf = Transform.new()
trf:setOrigin(Vec4.new(0.000000, 11.057286, 0.000000, 0))
rot = Mat3x4.new()
rot:setAll(1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000)
trf:setRotation(rot)
trf:setScale(1.000000)
node:getSceneNodeBase():getMoveComponent():setLocalTransform(trf)

node = scene:newModelNode("room")
node:getSceneNodeBase():getModelComponent():loadModelResource("Assets/room_room_2c303a64377351de.ankimdl")
trf = Transform.new()
trf:setOrigin(Vec4.new(0.000000, 11.142166, 0.000000, 0))
rot = Mat3x4.new()
rot:setAll(1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000)
trf:setRotation(rot)
trf:setScale(9.000000)
node:getSceneNodeBase():getMoveComponent():setLocalTransform(trf)

node = scene:newPointLightNode("Lamp")
lcomp = node:getSceneNodeBase():getLightComponent()
lcomp:setDiffuseColor(Vec4.new(100.000000, 100.000000, 100.000000, 1))
lcomp:setShadowEnabled(1)
lcomp:setRadius(100.000000)
trf = Transform.new()
trf:setOrigin(Vec4.new(4.076245, 5.903862, -1.005454, 0))
rot = Mat3x4.new()
rot:setAll(1.000000, 0.000000, 0.000000, 0.000000, 0.000000, -0.000000, 1.000000, 0.000000, 0.000000, -1.000000, -0.000000, 0.000000)
trf:setRotation(rot)
trf:setScale(1.000000)
node:getSceneNodeBase():getMoveComponent():setLocalTransform(trf)

node = scene:newPerspectiveCameraNode("Camera")
scene:setActiveCameraNode(node:getSceneNodeBase())
frustumc = node:getSceneNodeBase():getFrustumComponent()
frustumc:setPerspective(0.100000, 200.000000, getMainRenderer():getAspectRatio() * 0.750416, 0.750416)
trf = Transform.new()
trf:setOrigin(Vec4.new(5.526846, 8.527484, -6.015655, 0))
rot = Mat3x4.new()
rot:setAll(-0.805081, -0.216096, 0.552401, 0.000000, 0.056206, 0.899296, 0.433714, 0.000000, -0.590496, 0.380223, -0.711860, 0.000000)
trf:setRotation(rot)
trf:setScale(1.000000)
node:getSceneNodeBase():getMoveComponent():setLocalTransform(trf)

node = scene:newModelNode("room.001")
node:getSceneNodeBase():getModelComponent():loadModelResource("Assets/room.001_room.red_99eb56f1f0b59f98.ankimdl")
trf = Transform.new()
trf:setOrigin(Vec4.new(0.000000, 11.142166, 0.000000, 0))
rot = Mat3x4.new()
rot:setAll(1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000)
trf:setRotation(rot)
trf:setScale(9.000000)
node:getSceneNodeBase():getMoveComponent():setLocalTransform(trf)

node = scene:newModelNode("room.002")
node:getSceneNodeBase():getModelComponent():loadModelResource("Assets/room.002_room.green_acfa0c3d40cf5fea.ankimdl")
trf = Transform.new()
trf:setOrigin(Vec4.new(0.000000, 11.142166, 0.000000, 0))
rot = Mat3x4.new()
rot:setAll(1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000)
trf:setRotation(rot)
trf:setScale(9.000000)
node:getSceneNodeBase():getMoveComponent():setLocalTransform(trf)

node = scene:newModelNode("room.003")
node:getSceneNodeBase():getModelComponent():loadModelResource("Assets/room.003_room.blue_1d4e9304c9ecd2fe.ankimdl")
trf = Transform.new()
trf:setOrigin(Vec4.new(0.000000, 11.142166, 0.000000, 0))
rot = Mat3x4.new()
rot:setAll(1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000)
trf:setRotation(rot)
trf:setScale(9.000000)
node:getSceneNodeBase():getMoveComponent():setLocalTransform(trf)
