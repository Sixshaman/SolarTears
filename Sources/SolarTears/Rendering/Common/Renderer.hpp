#pragma once

#include "../../Core/Window.hpp"
#include "../../Logging/LoggerQueue.hpp"
#include "Scene/BaseRenderableSceneBuilder.hpp"

/*	
	My manifesto:
	
	I don't want to make the best engine ever. Like someone will care about that. Optimally, I just want an engine I could experiment with.
	Vertex shader tesselation. Emissive lighting shaders. Young technologies that were created 2 days ago - I'll touch everything. Of course, 
	this will take all of my time, but I don't mind it. Ultimately, I will become the champion and surpass anyone! The hardest algorithms
	in this world will be implemented in this engine! Using it will be the greatest things ever! And all of it will be made by me.
	Nobody will stop me from reaching my goal. <3

	(The end) 
*/

class FrameGraphConfig;
class FrameGraphDescription;
class BaseRenderableScene;

class Renderer
{
public: 
	Renderer(LoggerQueue* loggerQueue);
	virtual ~Renderer();

	virtual void AttachToWindow(Window* window)      = 0;
	virtual void ResizeWindowBuffers(Window* window) = 0;

	virtual BaseRenderableScene* InitScene(const RenderableSceneDescription& sceneDescription, const std::unordered_map<std::string_view, SceneObjectLocation>& sceneMeshInitialLocations, std::unordered_map<std::string_view, RenderableSceneObjectHandle>& outObjectHandles) = 0;
	virtual void InitFrameGraph(FrameGraphConfig&& frameGraphConfig, FrameGraphDescription&& frameGraphDescription)                                                                                                                                                             = 0;

	virtual void Render() = 0;

protected:
	LoggerQueue* mLoggingBoard;
};