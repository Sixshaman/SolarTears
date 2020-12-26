#pragma once

#include "../Core/Window.hpp"
#include "../Core/Scene/SceneDescription.hpp"
#include "../Logging/LoggerQueue.hpp"

/*	
	My manifesto:
	
	I don't want to make the best engine ever. Like someone will care about that. Optimally, I just want an engine I could experiment with.
	Vertex shader tesselation. Emissive lighting shaders. Young technologies that were created 2 days ago - I'll touch everything. Of course, 
	this will take all of my time, but I don't mind it. Ultimately, I will become the champion and surpass anyone! The hardest algorithms
	in this world will be implemented in this engine! Using it will be the greatest things ever! And all of it will be made by me.
	Nobody, not even John K, will stop me from reaching my goal.

	(The end) 
*/

class Renderer
{
public: 
	Renderer(LoggerQueue* loggerQueue);
	virtual ~Renderer();

	virtual void AttachToWindow(Window* window)      = 0;
	virtual void ResizeWindowBuffers(Window* window) = 0;

	virtual void InitSceneAndFrameGraph(SceneDescription* scene) = 0;
	virtual void RenderScene()                                   = 0;

	virtual uint64_t GetFrameNumber() const = 0;

protected:
	LoggerQueue* mLoggingBoard;
};