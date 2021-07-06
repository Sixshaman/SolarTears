#pragma once

#ifdef WIN32
#include <Windows.h>
#endif

#include <GL/GL.h>
#include <GL/glext.h>
#include <vector>
#include <string>
#include "../../Common/Scene/ClassicRenderableScene.hpp"
#include "../../../Core/FrameCounter.hpp"

namespace OpenGL
{
	class GLContext;

	class RenderableScene: public ClassicRenderableScene
	{
		friend class RenderableSceneBuilder;
		friend class SceneDescriptorCreator;

	public:
		RenderableScene();
		~RenderableScene();

		void FinalizeSceneUpdating() override final;

	public:
		void DrawObjectsOntoGBuffer() const;

	private:
		GLuint mSceneVertexArray;
		GLuint mSceneElementBuffer;

		GLuint mSceneUniformBuffer; //Common buffer for all uniform buffer data

		std::vector<GLuint> mSceneTextures;
	};
}