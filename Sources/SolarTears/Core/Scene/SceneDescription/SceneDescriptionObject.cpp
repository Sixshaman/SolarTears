#include "SceneDescriptionObject.hpp"
#include "MeshComponent.hpp"

static enum class SceneObjectFlags: uint64_t
{
	Static = 0x1, //The object never moves
};

SceneDescriptionObject::SceneDescriptionObject(uint64_t entityId): mEntityId(entityId)
{
	SetStatic(true);

	mLocation = SceneObjectLocation
	{
		.Position = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		.Scale = 1.0f,
		.RotationQuaternion = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f),
	};

	mMeshComponentIndex   = (uint32_t)(-1);
	mCameraComponentIndex = (uint32_t)(-1);
}

SceneDescriptionObject::~SceneDescriptionObject()
{
}

uint64_t SceneDescriptionObject::GetEntityId() const
{
	return mEntityId;
}

void SceneDescriptionObject::SetLocation(const SceneObjectLocation& location)
{
	mLocation = location;
}

SceneObjectLocation& SceneDescriptionObject::GetLocation()
{
	return mLocation;
}

const SceneObjectLocation& SceneDescriptionObject::GetLocation() const
{
	return mLocation;
}

bool SceneDescriptionObject::IsStatic() const
{
	return mFlags & (uint64_t)SceneObjectFlags::Static;
}

void SceneDescriptionObject::SetStatic(bool isStatic)
{
	mFlags ^= (-(uint64_t)isStatic ^ mFlags) & (uint64_t)SceneObjectFlags::Static;
}