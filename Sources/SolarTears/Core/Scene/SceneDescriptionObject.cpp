#include "SceneDescriptionObject.hpp"

SceneDescriptionObject::SceneDescriptionObject(uint64_t entityId): mEntityId(entityId)
{
}

SceneDescriptionObject::~SceneDescriptionObject()
{
}

void SceneDescriptionObject::SetPosition(DirectX::XMVECTOR position)
{
	DirectX::XMStoreFloat3(&mLocation.Position, position);
}

void SceneDescriptionObject::SetRotation(DirectX::XMVECTOR rotation)
{
	DirectX::XMStoreFloat4(&mLocation.RotationQuaternion, rotation);
}

void SceneDescriptionObject::SetScale(float scale)
{
	mLocation.Scale = scale;
}

DirectX::XMVECTOR SceneDescriptionObject::GetPosition() const
{
	return DirectX::XMLoadFloat3(&mLocation.Position);
}

DirectX::XMVECTOR SceneDescriptionObject::GetRotation() const
{
	return DirectX::XMLoadFloat4(&mLocation.RotationQuaternion);
}

float SceneDescriptionObject::GetScale() const
{
	return mLocation.Scale;
}

SceneDescriptionObject::SceneDescriptionObject(SceneDescriptionObject&& right) noexcept
{
	*this = std::move(right);
}

SceneDescriptionObject& SceneDescriptionObject::operator=(SceneDescriptionObject&& right) noexcept
{
	mEntityId = right.mEntityId;

	mLocation = right.mLocation;

	mMeshComponent = std::move(right.mMeshComponent);

	return *this;
}

uint64_t SceneDescriptionObject::GetEntityId() const
{
	return mEntityId;
}

void SceneDescriptionObject::SetMeshComponent(const MeshComponent& meshComponent)
{
	mMeshComponent = std::make_unique<MeshComponent>(meshComponent);
}

void SceneDescriptionObject::SetInputComponent(const InputComponent& inputComponent)
{
	mInputComponent = std::make_unique<InputComponent>(inputComponent);
}

SceneDescriptionObject::MeshComponent* SceneDescriptionObject::GetMeshComponent() const
{
	return mMeshComponent.get();
}

SceneDescriptionObject::InputComponent* SceneDescriptionObject::GetInputComponent() const
{
	return mInputComponent.get();
}
