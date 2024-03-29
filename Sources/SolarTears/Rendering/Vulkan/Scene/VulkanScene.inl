template<typename SubmeshCallback>
void RenderableScene::DrawStaticObjects(VkCommandBuffer commandBuffer, SubmeshCallback submeshCallback) const
{
	for(uint32_t meshIndex = mStaticUniqueMeshSpan.Begin; meshIndex < mStaticUniqueMeshSpan.End; meshIndex++)
	{
		for(uint32_t submeshIndex = mSceneMeshes[meshIndex].FirstSubmeshIndex; submeshIndex < mSceneMeshes[meshIndex].AfterLastSubmeshIndex; submeshIndex++)
		{
			submeshCallback(commandBuffer, mSceneSubmeshes[submeshIndex].MaterialIndex);
			vkCmdDrawIndexed(commandBuffer, mSceneSubmeshes[submeshIndex].IndexCount, mSceneMeshes[meshIndex].InstanceCount, mSceneSubmeshes[submeshIndex].FirstIndex, mSceneSubmeshes[submeshIndex].VertexOffset, 0);
		}
	}
}

template<typename MeshCallback, typename SubmeshCallback>
void RenderableScene::DrawNonStaticObjects(VkCommandBuffer commandBuffer, MeshCallback meshCallback, SubmeshCallback submeshCallback) const
{
	for(uint32_t meshIndex = mNonStaticMeshSpan.Begin; meshIndex < mNonStaticMeshSpan.End; meshIndex++)
	{
		meshCallback(commandBuffer, mSceneMeshes[meshIndex].PerObjectDataIndex);
		for(uint32_t submeshIndex = mSceneMeshes[meshIndex].FirstSubmeshIndex; submeshIndex < mSceneMeshes[meshIndex].AfterLastSubmeshIndex; submeshIndex++)
		{
			submeshCallback(commandBuffer, mSceneSubmeshes[submeshIndex].MaterialIndex);
			vkCmdDrawIndexed(commandBuffer, mSceneSubmeshes[submeshIndex].IndexCount, mSceneMeshes[meshIndex].InstanceCount, mSceneSubmeshes[submeshIndex].FirstIndex, mSceneSubmeshes[submeshIndex].VertexOffset, 0);
		}
	}
}