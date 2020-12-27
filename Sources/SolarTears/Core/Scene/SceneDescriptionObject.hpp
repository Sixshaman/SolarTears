#pragma once

#include <vector>
#include <string>
#include <memory>
#include "Scene.hpp"
#include "../../../3rd party/DirectXMath/Inc/DirectXMath.h"

class SceneDescriptionObject
{
public:
	struct SceneObjectVertex
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT2 Texcoord;
	};

	struct MeshComponent
	{
		std::vector<SceneObjectVertex> Vertices;
		std::vector<uint32_t>          Indices;
		std::wstring                   TextureFilename;
	};

public:
	SceneDescriptionObject(uint64_t entityId);
	~SceneDescriptionObject();

	SceneDescriptionObject(const SceneDescriptionObject&)            = delete;
	SceneDescriptionObject& operator=(const SceneDescriptionObject&) = delete;

	SceneDescriptionObject(SceneDescriptionObject&& right)      noexcept;
	SceneDescriptionObject& operator=(SceneDescriptionObject&&) noexcept;

	uint64_t GetEntityId() const;

	void SetPosition(DirectX::XMVECTOR position);
	void SetRotation(DirectX::XMVECTOR rotation);
	void SetScale(float scale);

	DirectX::XMVECTOR GetPosition() const;
	DirectX::XMVECTOR GetRotation() const;
	float GetScale()                const;

	void SetMeshComponent(const MeshComponent& meshComponent);

	MeshComponent* GetMeshComponent() const;

private:
	uint64_t mEntityId;

	Scene::SceneObjectLocation mLocation;

	std::unique_ptr<MeshComponent> mMeshComponent;
};