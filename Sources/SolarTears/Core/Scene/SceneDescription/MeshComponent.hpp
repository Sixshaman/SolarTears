#pragma once

#include <DirectXMath.h>
#include <vector>
#include <string>

struct SceneObjectVertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 Texcoord;
};

struct SceneObjectMeshComponent
{
	std::vector<SceneObjectVertex> Vertices;
	std::vector<uint32_t>          Indices;
	std::wstring                   TextureFilename;
};