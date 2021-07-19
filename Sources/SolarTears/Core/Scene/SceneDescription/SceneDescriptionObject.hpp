#pragma once

#include <vector>
#include <string>
#include <memory>

class MeshComponent;
class LocationComponent;

class SceneDescriptionObject
{
public:
	SceneDescriptionObject(uint64_t entityId);
	~SceneDescriptionObject();

	SceneDescriptionObject(const SceneDescriptionObject&)            = delete;
	SceneDescriptionObject& operator=(const SceneDescriptionObject&) = delete;

	SceneDescriptionObject(SceneDescriptionObject&& right)      noexcept;
	SceneDescriptionObject& operator=(SceneDescriptionObject&&) noexcept;

	uint64_t GetEntityId() const;

	void SetMeshComponent(const MeshComponent& meshComponent);
	void SetLocationComponent(const LocationComponent& locationComponent);

	MeshComponent*     GetMeshComponent()     const;
	LocationComponent* GetLocationComponent() const;

private:
	uint64_t mEntityId;

	std::unique_ptr<MeshComponent>     mMeshComponent;
	std::unique_ptr<LocationComponent> mLocationComponent;
};