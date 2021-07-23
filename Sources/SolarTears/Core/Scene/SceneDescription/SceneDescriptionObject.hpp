#pragma once

#include <vector>
#include <string>
#include <memory>
#include "../SceneObjectLocation.hpp"

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

	std::string GetMeshComponentName() const;
	void SetMeshComponentName(const std::string& meshComponentName);

private:
	uint64_t mEntityId;

	SceneObjectLocation mLocation; //All scene objects have a location

	std::string mMeshComponentName;
};