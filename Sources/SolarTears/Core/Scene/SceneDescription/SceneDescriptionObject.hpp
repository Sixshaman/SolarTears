#pragma once

#include <vector>
#include <string>
#include <memory>
#include "../SceneObjectLocation.hpp"

class SceneDescriptionObject
{
public:
	SceneDescriptionObject();
	~SceneDescriptionObject();

	SceneDescriptionObject(const SceneDescriptionObject&)            = delete;
	SceneDescriptionObject& operator=(const SceneDescriptionObject&) = delete;

	SceneDescriptionObject(SceneDescriptionObject&& right)      = default;
	SceneDescriptionObject& operator=(SceneDescriptionObject&&) = default;

	void SetLocation(const SceneObjectLocation& location);
	SceneObjectLocation& GetLocation();
	const SceneObjectLocation& GetLocation() const;

	const std::string& GetMeshComponentName() const;
	void SetMeshComponentName(const std::string& meshComponentName);

private:
	SceneObjectLocation mLocation; //All scene objects have a location

	std::string mMeshComponentName;
};