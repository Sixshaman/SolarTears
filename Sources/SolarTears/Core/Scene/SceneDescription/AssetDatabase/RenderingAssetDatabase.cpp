#include "RenderingAssetDatabase.hpp"

RenderingAssetDatabase::RenderingAssetDatabase()
{
}

RenderingAssetDatabase::~RenderingAssetDatabase()
{
}

void RenderingAssetDatabase::AddMaterial(std::string name, const SceneMaterialAsset& materialAsset)
{
	mMaterials[name] = materialAsset;
}

SceneMaterialAsset& RenderingAssetDatabase::GetMaterial(std::string name)
{
	return mMaterials.at(name);
}

const SceneMaterialAsset& RenderingAssetDatabase::GetMaterial(std::string name) const
{
	return mMaterials.at(name);
}