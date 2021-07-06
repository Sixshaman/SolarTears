#include "Engine.hpp"
#include "FrameCounter.hpp"
#include "FPSCounter.hpp"
#include "Scene/SceneDescription.hpp"
#include "Scene/Scene.hpp"
#include "../Input/Inputter.hpp"
#include "../Rendering/Common/FrameGraph/FrameGraphConfig.hpp"
#include "../Rendering/Common/FrameGraph/FrameGraphDescription.hpp"
#include "../Rendering/Vulkan/VulkanRenderer.hpp"
#include "../Rendering/D3D12/D3D12Renderer.hpp"
#include "../Rendering/OGL/OGLRenderer.hpp"
#include "../Logging/LoggerQueue.hpp"
#include "../Logging/VisualStudioDebugLogger.hpp"

#include "../Rendering/Common/FrameGraph/Passes/CopyImagePass.hpp"
#include "../Rendering/Common/FrameGraph/Passes/GBufferPass.hpp"

Engine::Engine(): mPaused(false)
{
	mLoggerQueue = std::make_unique<LoggerQueue>();
	mLogger      = std::make_unique<VisualStudioDebugLogger>();

	mTimer      = std::make_unique<Timer>();
	mThreadPool = std::make_unique<ThreadPool>();

	mFrameCounter = std::make_unique<FrameCounter>();
	mFPSCounter   = std::make_unique<FPSCounter>();

	mRenderingSystem = std::make_unique<OpenGL::Renderer>(mLoggerQueue.get(), mFrameCounter.get());
	mInputSystem     = std::make_unique<Inputter>(mLoggerQueue.get());
}

Engine::~Engine()
{
}

void Engine::Start()
{
	CreateScene();
}

void Engine::BindToWindow(Window* window)
{
	mRenderingSystem->AttachToWindow(window);
	mInputSystem->AttachToWindow(window);

	CreateFrameGraph(window);
		
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
		that->CreateFrameGraph(window);
		that->mPaused = false;
	});
}

void Engine::Update()
{
	const uint32_t maxLogMessagesPerTick = 10;
	mLoggerQueue->FeedMessages(mLogger.get(), maxLogMessagesPerTick);

	mInputSystem->UpdateControls();
	if(mInputSystem->GetKeyStateChange(ControlCode::Pause))
	{
		mPaused = !mPaused;
		mInputSystem->SetPaused(mPaused);
	}

	if(!mPaused)
	{
		mTimer->Tick();

		mScene->ProcessControls(mInputSystem.get(), mTimer->GetDeltaTime());

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

	meshComponent.TextureFilename = L"../Assets/Textures/Test1.dds";
	sceneObject.SetMeshComponent(meshComponent);

	mRenderingSystem->InitScene(&sceneDesc);

	sceneDesc.BuildScene(mScene.get());
}

void Engine::CreateFrameGraph(Window* window)
{
	FrameGraphConfig frameGraphConfig;
	frameGraphConfig.SetScreenSize((uint16_t)window->GetWidth(), (uint16_t)window->GetHeight());


	FrameGraphDescription frameGraphDescription;

	frameGraphDescription.AddRenderPass(GBufferPassBase::PassType,   "GBuffer");
	frameGraphDescription.AddRenderPass(CopyImagePassBase::PassType, "CopyImage");

	frameGraphDescription.AssignSubresourceName("GBuffer",   GBufferPassBase::ColorBufferImageId, "ColorBuffer");
	frameGraphDescription.AssignSubresourceName("CopyImage", CopyImagePassBase::SrcImageId,       "ColorBuffer");
	frameGraphDescription.AssignSubresourceName("CopyImage", CopyImagePassBase::DstImageId,       "Backbuffer");

	frameGraphDescription.AssignBackbufferName("Backbuffer");


	mRenderingSystem->InitFrameGraph(std::move(frameGraphConfig), std::move(frameGraphDescription));
}
