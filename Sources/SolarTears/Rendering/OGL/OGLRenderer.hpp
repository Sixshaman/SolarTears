#pragma once

#include "../Common/Renderer.hpp"
#include "../Common/RenderingUtils.hpp"
#include <GL/GL.h>
#include <memory>

class FrameCounter;

namespace OpenGL
{
	class RenderableScene;
	class GLContext;

	class Renderer: public ::Renderer
	{
	public:
		Renderer(LoggerQueue* loggerQueue, FrameCounter* frameCounter);
		~Renderer();

		void AttachToWindow(Window* window)      override;
		void ResizeWindowBuffers(Window* window) override;

		void InitScene(SceneDescription* sceneDescription) override;
		void RenderScene()                                 override;

		void InitFrameGraph(FrameGraphConfig&& frameGraphConfig, FrameGraphDescription&& frameGraphDescription) override;

	private:
		const FrameCounter* mFrameCounterRef;

		std::unique_ptr<GLContext> mGLContext;
	};
}