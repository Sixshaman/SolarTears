#pragma once

#include <vector>
#include <string>
#include <memory>
#include "../SceneObjectLocation.hpp"
#include "MeshComponent.hpp"
#include "CameraComponent.hpp"

class SceneDescriptionObject
{
public:
	SceneDescriptionObject(uint64_t entityId);
	~SceneDescriptionObject();

	SceneDescriptionObject(const SceneDescriptionObject&)            = delete;
	SceneDescriptionObject& operator=(const SceneDescriptionObject&) = delete;

	SceneDescriptionObject(SceneDescriptionObject&& right)      = default;
	SceneDescriptionObject& operator=(SceneDescriptionObject&&) = default;

	uint64_t GetEntityId() const;

	void SetLocation(const SceneObjectLocation& location);
	SceneObjectLocation& GetLocation();
	const SceneObjectLocation& GetLocation() const;

	bool IsStatic() const;
	void SetStatic(bool isStatic);

private:
	uint64_t mEntityId;
	uint64_t mFlags;

	SceneObjectLocation mLocation; //All scene objects have a location component

	uint32_t mMeshComponentIndex;
	uint32_t mCameraComponentIndex;
};