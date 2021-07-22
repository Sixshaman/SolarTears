#pragma once

#include "SceneMaterialAsset.hpp"
#include <unordered_map>

class RenderingAssetDatabase
{
public:
	RenderingAssetDatabase();
	~RenderingAssetDatabase();

	void AddMaterial(std::string name, const SceneMaterialAsset& materialAsset);
	SceneMaterialAsset& GetMaterial(std::string name);
	const SceneMaterialAsset& GetMaterial(std::string name) const;

private:
	std::unordered_map<std::string, SceneMaterialAsset> mMaterials;
};