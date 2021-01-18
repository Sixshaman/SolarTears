#include "Engine.hpp"
#include "Scene/SceneDescription.hpp"
#include "Scene/Scene.hpp"
#include "../Rendering/VulkanC/VulkanCRenderer.hpp"
#include "../Logging/LoggerQueue.hpp"
#include "../Logging/VisualStudioDebugLogger.hpp"

Engine::Engine(): mPaused(false)
{
	mLoggerQueue = std::make_unique<LoggerQueue>();
	mLogger      = std::make_unique<VisualStudioDebugLogger>();

	mTimer      = std::make_unique<Timer>();
	mThreadPool = std::make_unique<ThreadPool>();

	mRenderingSystem = std::make_unique<VulkanCBindings::Renderer>(mLoggerQueue.get(), mThreadPool.get());
}

Engine::~Engine()
{
}

void Engine::BindToWindow(Window* window)
{
	mRenderingSystem->AttachToWindow(window);
	CreateScene();

	window->RegisterResizeStartedCallback([](Window* window, void* userObject)
	{
		UNREFERENCED_PARAMETER(window);

		Engine* that  = reinterpret_cast<Engine*>(userObject);
		that->mPaused = true;
	}, this);

	window->RegisterResizeFinishedCallback([](Window* window, void* userObject)
	{
		Engine* that = reinterpret_cast<Engine*>(userObject);
		that->mRenderingSystem->ResizeWindowBuffers(window);
		that->mPaused = false;
	}, this);
}

void Engine::Update()
{
	if(!mPaused)
	{
		mTimer->Tick();

		const uint32_t maxLogMessagesPerTick = 10;
		mLoggerQueue->FeedMessages(mLogger.get(), maxLogMessagesPerTick);

		mRenderingSystem->RenderScene();

		CalcNextFPS();
	}
}

void Engine::CreateScene()
{
	SceneDescription sceneDesc;

	SceneDescriptionObject& sceneObject = sceneDesc.CreateEmptySceneObject();
	sceneObject.SetPosition(DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f));
	sceneObject.SetRotation(DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f));
	sceneObject.SetScale(1.0f);

	SceneDescriptionObject::MeshComponent meshComponent;

	DirectX::XMFLOAT3 objectPositions[] =
	{
		DirectX::XMFLOAT3(-1.0f, -1.0f,  1.0f),
		DirectX::XMFLOAT3(-1.0f,  1.0f,  1.0f),
		DirectX::XMFLOAT3( 1.0f,  1.0f,  1.0f),
		DirectX::XMFLOAT3( 1.0f, -1.0f,  1.0f)
	};

	DirectX::XMFLOAT3 objectNormals[] =
	{
		DirectX::XMFLOAT3( 0.0f,  0.0f, -1.0f),
		DirectX::XMFLOAT3( 0.0f,  0.0f, -1.0f),
		DirectX::XMFLOAT3( 0.0f,  0.0f, -1.0f),
		DirectX::XMFLOAT3( 0.0f,  0.0f, -1.0f)
	};

	DirectX::XMFLOAT2 objectTexcoords[] =
	{
		DirectX::XMFLOAT2(0.0f, 1.0f),
		DirectX::XMFLOAT2(0.0f, 0.0f),
		DirectX::XMFLOAT2(1.0f, 0.0f),
		DirectX::XMFLOAT2(1.0f, 1.0f)
	};

	for(size_t i = 0; i < 4; i++)
	{
		SceneDescriptionObject::SceneObjectVertex vertex;
		vertex.Position = objectPositions[i];
		vertex.Normal   = objectNormals[i];
		vertex.Texcoord = objectTexcoords[i];

		meshComponent.Vertices.push_back(vertex);
	}

	meshComponent.Indices =
	{
		0, 1, 2,
		0, 2, 3
	};

	meshComponent.TextureFilename = L"Test1.dds";
	sceneObject.SetMeshComponent(meshComponent);

	mRenderingSystem->InitSceneAndFrameGraph(&sceneDesc);
}

void Engine::CalcNextFPS()
{
	static float lastMeasurementTime = mTimer->GetCurrTime();
	float currMeasurementTime = mTimer->GetCurrTime();

	static uint64_t lastFrameIndex = mRenderingSystem->GetFrameNumber();
	uint64_t frameIndex = mRenderingSystem->GetFrameNumber();

	if(currMeasurementTime - lastMeasurementTime >= 1.0f)
	{
		float currentFPS    = (frameIndex - lastFrameIndex) / (currMeasurementTime - lastMeasurementTime);
		lastMeasurementTime = currMeasurementTime;
		lastFrameIndex      = frameIndex;

		mLoggerQueue->PostLogMessage("FPS: " + std::to_string(currentFPS) + ", mspf: " + std::to_string(1.0f / currentFPS));
	}
}