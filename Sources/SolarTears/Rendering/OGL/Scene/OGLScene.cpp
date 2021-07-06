#include "OGLScene.hpp"
#include "../OGLFunctions.hpp"
#include "../OGLUtils.hpp"

OpenGL::RenderableScene::RenderableScene(): ClassicRenderableScene()
{
}

OpenGL::RenderableScene::~RenderableScene()
{
}

void OpenGL::RenderableScene::FinalizeSceneUpdating()
{
	glMapNamedBuffer(mSceneUniformBuffer, GL_WRITE_ONLY);

	FinalizeSceneUpdatingImpl();

	glUnmapNamedBuffer(mSceneUniformBuffer);
}

void OpenGL::RenderableScene::DrawObjectsOntoGBuffer() const
{
	glBindVertexArray(mSceneVertexArray);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mSceneElementBuffer);

	for(size_t meshIndex = 0; meshIndex < mSceneMeshes.size(); meshIndex++)
	{
		for(uint32_t subobjectIndex = mSceneMeshes[meshIndex].FirstSubobjectIndex; subobjectIndex < mSceneMeshes[meshIndex].AfterLastSubobjectIndex; subobjectIndex++)
		{
			//glBindBufferRange(GL_UNIFORM_BUFFER, mGBufferUniformFrameIndex, mSceneUniformBuffer, subobjectIndex, sizeof(PerObjectData));
			glDrawElementsBaseVertex(GL_TRIANGLES, mSceneSubobjects[subobjectIndex].IndexCount, OGLUtils::FormatForIndexType<RenderableSceneIndex>, VoidPtrOffset(mSceneSubobjects[subobjectIndex].FirstIndex), mSceneSubobjects[subobjectIndex].VertexOffset);
		}
	}

	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}