template<typename SubmeshCallback>
inline void D3D12::RenderableScene::DrawObjectsWithBakedData(ID3D12GraphicsCommandList* cmdList, SubmeshCallback submeshCallback) const
{
	for(uint32_t meshIndex = mStaticMeshSpan.Begin; meshIndex < mStaticMeshSpan.End; meshIndex++)
	{
		for(uint32_t submeshIndex = mSceneMeshes[meshIndex].FirstSubmeshIndex; submeshIndex < mSceneMeshes[meshIndex].AfterLastSubmeshIndex; submeshIndex++)
		{
			submeshCallback(cmdList, mSceneSubmeshes[submeshIndex]);
			cmdList->DrawIndexedInstanced(mSceneSubmeshes[submeshIndex].IndexCount, mSceneMeshes[meshIndex].InstanceCount, mSceneSubmeshes[submeshIndex].FirstIndex, mSceneSubmeshes[submeshIndex].VertexOffset, 0);
		}
	}
}

template<typename MeshCallback, typename SubmeshCallback>
inline void D3D12::RenderableScene::DrawObjectsWithNonBakedData(ID3D12GraphicsCommandList* cmdList, MeshCallback meshCallback, SubmeshCallback submeshCallback) const
{
	//The spans should follow each other
	assert(mStaticInstancedMeshSpan.End == mRigidMeshSpan.Begin);

	const uint32_t firstMeshIndex     = mStaticInstancedMeshSpan.Begin;
	const uint32_t afterLastMeshIndex = mRigidMeshSpan.End;

	for(uint32_t meshIndex = firstMeshIndex; meshIndex < afterLastMeshIndex; meshIndex++)
	{
		meshCallback(cmdList, mSceneMeshes[meshIndex].PerObjectDataIndex);
		for(uint32_t submeshIndex = mSceneMeshes[meshIndex].FirstSubmeshIndex; submeshIndex < mSceneMeshes[meshIndex].AfterLastSubmeshIndex; submeshIndex++)
		{
			submeshCallback(cmdList, mSceneSubmeshes[submeshIndex].MaterialIndex);
			cmdList->DrawIndexedInstanced(mSceneSubmeshes[submeshIndex].IndexCount, mSceneMeshes[meshIndex].InstanceCount, mSceneSubmeshes[submeshIndex].FirstIndex, mSceneSubmeshes[submeshIndex].VertexOffset, 0);
		}
	}
}