#include "SceneDescriptionObject.hpp"
#include "LocationComponent.hpp"
#include "MeshComponent.hpp"

SceneDescriptionObject::SceneDescriptionObject(uint64_t entityId): mEntityId(entityId)
{
}

SceneDescriptionObject::~SceneDescriptionObject()
{
}

SceneDescriptionObject::SceneDescriptionObject(SceneDescriptionObject&& right) noexcept
{
	*this = std::move(right);
}

SceneDescriptionObject& SceneDescriptionObject::operator=(SceneDescriptionObject&& right) noexcept
{
	mEntityId = right.mEntityId;

	mMeshComponent     = std::move(right.mMeshComponent);
	mLocationComponent = std::move(right.mLocationComponent);

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

void SceneDescriptionObject::SetLocationComponent(const LocationComponent& locationComponent)
{
	mLocationComponent = std::make_unique<LocationComponent>(locationComponent);
}

MeshComponent* SceneDescriptionObject::GetMeshComponent() const
{
	return mMeshComponent.get();
}

LocationComponent* SceneDescriptionObject::GetLocationComponent() const
{
	return mLocationComponent.get();
}