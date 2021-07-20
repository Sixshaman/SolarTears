#pragma once

#include <vector>
#include <string>
#include <memory>
#include "../SceneObjectLocation.hpp"

class MeshComponent;

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

	void SetLocation(const SceneObjectLocation& location);
	SceneObjectLocation& GetLocation();
	const SceneObjectLocation& GetLocation() const;

	void SetMeshComponent(const MeshComponent& meshComponent);
	MeshComponent* GetMeshComponent() const;

private:
	uint64_t mEntityId;

	SceneObjectLocation mLocation; //All scene objects have a location component

	std::unique_ptr<MeshComponent> mMeshComponent;
};