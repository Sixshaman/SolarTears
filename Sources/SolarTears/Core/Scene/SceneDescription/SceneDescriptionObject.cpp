#include "SceneDescriptionObject.hpp"

SceneDescriptionObject::SceneDescriptionObject(uint64_t entityId): mEntityId(entityId)
{
	mLocation = SceneObjectLocation
	{
		.Position = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		.Scale = 1.0f,
		.RotationQuaternion = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f),
	};
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

const std::string& SceneDescriptionObject::GetMeshComponentName() const
{
	return mMeshComponentName;
}

void SceneDescriptionObject::SetMeshComponentName(const std::string& meshComponentName)
{
	mMeshComponentName = meshComponentName;
}