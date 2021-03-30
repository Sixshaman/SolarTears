#include "Engine.hpp"
#include "FrameCounter.hpp"
#include "FPSCounter.hpp"
#include "Scene/SceneDescription.hpp"
#include "Scene/Scene.hpp"
#include "../Input/Inputter.hpp"
#include "../Rendering/VulkanC/VulkanCRenderer.hpp"
#include "../Logging/LoggerQueue.hpp"
#include "../Logging/VisualStudioDebugLogger.hpp"

Engine::Engine(): mPaused(false)
{
	mLoggerQueue = std::make_unique<LoggerQueue>();
	mLogger      = std::make_unique<VisualStudioDebugLogger>();

	mTimer      = std::make_unique<Timer>();
	mThreadPool = std::make_unique<ThreadPool>();

	mFrameCounter = std::make_unique<FrameCounter>();
	mFPSCounter   = std::make_unique<FPSCounter>();

	mRenderingSystem = std::make_unique<VulkanCBindings::Renderer>(mLoggerQueue.get(), mFrameCounter.get(), mThreadPool.get());
	mInputSystem     = std::make_unique<Inputter>(mLoggerQueue.get());

	CreateScene();
}

Engine::~Engine()
{
}

void Engine::BindToWindow(Window* window)
{
	mRenderingSystem->AttachToWindow(window);
	mInputSystem->AttachToWindow(window);

	window->SetResizeCallbackUserPtr(this);

	window->RegisterResizeStartedCallback([](Window* window, void* userObject)
	{
		UNREFERENCED_PARAMETER(window);

		Engine* that  = reinterpret_cast<Engine*>(userObject);
		that->mPaused = true;
	});

	window->RegisterResizeFinishedCallback([](Window* window, void* userObject)
	{
		Engine* that = reinterpret_cast<Engine*>(userObject);
		that->mRenderingSystem->ResizeWindowBuffers(window);
		that->mPaused = false;
	});
}

void Engine::Update()
{
	if(!mPaused)
	{
		mTimer->Tick();

		const uint32_t maxLogMessagesPerTick = 10;
		mLoggerQueue->FeedMessages(mLogger.get(), maxLogMessagesPerTick);

		mInputSystem->UpdateScene(mTimer->GetDeltaTime());

		mScene->UpdateScene();
		mRenderingSystem->RenderScene();

		mFrameCounter->IncrementFrame();
		mFPSCounter->LogFPS(mFrameCounter.get(), mTimer.get(), mLoggerQueue.get());
	}
}

void Engine::CreateScene()
{
	mScene.reset();
	mScene = std::make_unique<Scene>();

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

	SceneDescriptionObject& cameraObject = sceneDesc.GetCameraSceneObject();
	
	SceneDescriptionObject::InputComponent cameraInputComponent;
	std::fill(cameraInputComponent.KeyPressedCallbacks, cameraInputComponent.KeyPressedCallbacks + (uint8_t)ControlCode::Count, nullptr);
	cameraInputComponent.AxisMoveCallback1 = nullptr;
	cameraInputComponent.AxisMoveCallback3 = nullptr;

	cameraInputComponent.KeyPressedCallbacks[(uint8_t)ControlCode::MoveForward] = [](InputControlLocation* location, float dt)
	{
		location->Walk(10.0f * dt);
	};
	cameraInputComponent.KeyPressedCallbacks[(uint8_t)ControlCode::MoveBack] = [](InputControlLocation* location, float dt)
	{
		location->Walk(-10.0f * dt);
	};
	cameraInputComponent.KeyPressedCallbacks[(uint8_t)ControlCode::MoveLeft] = [](InputControlLocation* location, float dt)
	{
		location->Strafe(-10.0f * dt);
	};
	cameraInputComponent.KeyPressedCallbacks[(uint8_t)ControlCode::MoveRight] = [](InputControlLocation* location, float dt)
	{
		location->Strafe(10.0f * dt);
	};
	cameraInputComponent.AxisMoveCallback2 = [](InputControlLocation* location, float dx, float dy, float dt)
	{
		location->Pitch(-dy * dt);
		location->Rotate(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), dx * dt);
	};
	 
	cameraObject.SetInputComponent(cameraInputComponent);

	mRenderingSystem->InitScene(&sceneDesc);
	mInputSystem->InitScene(&sceneDesc);

	sceneDesc.BuildScene(mScene.get());
}
