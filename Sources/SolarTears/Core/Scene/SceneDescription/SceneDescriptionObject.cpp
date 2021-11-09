#include "SceneDescriptionObject.hpp"

SceneDescriptionObject::SceneDescriptionObject()
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