template<typename SubmeshCallback>
inline void RenderableScene::DrawStaticObjects(ID3D12GraphicsCommandList* cmdList, SubmeshCallback submeshCallback) const
{
	for(uint32_t meshIndex = mStaticUniqueMeshSpan.Begin; meshIndex < mStaticUniqueMeshSpan.End; meshIndex++)
	{
		for(uint32_t submeshIndex = mSceneMeshes[meshIndex].FirstSubmeshIndex; submeshIndex < mSceneMeshes[meshIndex].AfterLastSubmeshIndex; submeshIndex++)
		{
			submeshCallback(cmdList, mSceneSubmeshes[submeshIndex].MaterialIndex);
			cmdList->DrawIndexedInstanced(mSceneSubmeshes[submeshIndex].IndexCount, mSceneMeshes[meshIndex].InstanceCount, mSceneSubmeshes[submeshIndex].FirstIndex, mSceneSubmeshes[submeshIndex].VertexOffset, 0);
		}
	}
}

template<typename MeshCallback, typename SubmeshCallback>
inline void RenderableScene::DrawNonStaticObjects(ID3D12GraphicsCommandList* cmdList, MeshCallback meshCallback, SubmeshCallback submeshCallback) const
{
	for(uint32_t meshIndex = mNonStaticMeshSpan.Begin; meshIndex < mNonStaticMeshSpan.End; meshIndex++)
	{
		meshCallback(cmdList, mSceneMeshes[meshIndex].PerObjectDataIndex);
		for(uint32_t submeshIndex = mSceneMeshes[meshIndex].FirstSubmeshIndex; submeshIndex < mSceneMeshes[meshIndex].AfterLastSubmeshIndex; submeshIndex++)
		{
			submeshCallback(cmdList, mSceneSubmeshes[submeshIndex].MaterialIndex);
			cmdList->DrawIndexedInstanced(mSceneSubmeshes[submeshIndex].IndexCount, mSceneMeshes[meshIndex].InstanceCount, mSceneSubmeshes[submeshIndex].FirstIndex, mSceneSubmeshes[submeshIndex].VertexOffset, 0);
		}
	}
}