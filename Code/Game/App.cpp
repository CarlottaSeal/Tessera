#include "Engine/Window/Window.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VulkanRenderer.h"
#include "Engine/Renderer/VulkanDeferredPath.h"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/SimpleTriangleFont.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "Engine/Input/KeyButtonState.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Audio/AudioSystem.hpp"

#include "Game/App.hpp"
#include "Game/Game.hpp"
#include "Game/Gamecommon.hpp"
#include "Game/EngineBuildPreferences.hpp"

#include <math.h>

App* g_theApp = nullptr;
Renderer* g_theRenderer = nullptr;
VulkanDeferredPath* g_theDeferred = nullptr;
InputSystem* g_theInput = nullptr;
//AudioSystem* g_theAudio = nullptr;
Window* g_theWindow = nullptr;
Game* g_theGame = nullptr;

App::App()
{
}

App::~App()
{
}

void App::Startup()
{
	//Parse GameConfig
	XmlDocument gameConfigDoc;
	XmlResult loadResult = gameConfigDoc.LoadFile("Data/GameConfig.xml");
	UNUSED(loadResult)
	/*if (loadResult != XmlResult::XML_SUCCESS)
	{
		return;
	}*/
	XmlElement* rootElement = gameConfigDoc.RootElement();
	if (rootElement)
	{
		g_gameConfigBlackboard.PopulateFromXmlElementAttributes(*rootElement);

	}
	
	//Create all engine subsystems
	InputSystemConfig inputConfig;
	g_theInput = new InputSystem(inputConfig);

	WindowConfig windowConfig;
	windowConfig.m_aspectRatio = 2.f;
	windowConfig.m_inputSystem = g_theInput;
	windowConfig.m_windowTitle = g_gameConfigBlackboard.GetValue("windowTitle", "Protogame3D");
	windowConfig.m_isFullscreen = g_gameConfigBlackboard.GetValue("windowFullscreen", false);
	g_theWindow = new Window(windowConfig);

	RendererConfig rendererConfig;
	rendererConfig.m_window = g_theWindow;
	g_theRenderer = new Renderer(rendererConfig);

	EventSystemConfig eventSystemConfig;
	g_theEventSystem = new EventSystem(eventSystemConfig);

	DevConsoleConfig devConsoleConfig;
	devConsoleConfig.m_defaultRenderer = g_theRenderer;
	devConsoleConfig.m_defaultFontName = "SquirrelFixedFont";
	Camera* devCamera = new Camera();
	devCamera->SetOrthographicView(Vec2(0.f, 0.f), Vec2(1600.f, 800.f));
	//devConsoleConfig.m_camera = &g_theGame->m_screenCamera;
	devConsoleConfig.m_camera = devCamera;
	g_theDevConsole = new DevConsole(devConsoleConfig);

	/*AudioSystemConfig audioSystemConfig;
	g_theAudio = new AudioSystem(audioSystemConfig);*/

	DebugRenderConfig debugRenderConfig;
	debugRenderConfig.m_renderer = g_theRenderer;

	g_theWindow->Startup();
	g_theRenderer->Startup();
	//g_theAudio->Startup();
	g_theEventSystem->StartUp();
	g_theDevConsole->Startup();
	g_theInput->Startup();
	DebugRenderSystemStartup(debugRenderConfig);

#ifdef ENGINE_VULKAN_RENDERER
	// Bring up the tile-deferred path on top of the (now-stable) Vulkan renderer.
	g_theDeferred = new VulkanDeferredPath();
	g_theDeferred->Init(g_theRenderer->GetSubRenderer());
#endif

	g_theGame = new Game();
	g_theGame->m_gameClock = new Clock(Clock::GetSystemClock());

	g_theEventSystem->SubscribeEventCallBackFunction("quit", OnQuitEvent);

	g_theDevConsole->AddLine(Rgba8::BLUE, "Type help for a list of commands");
}

void App::Shutdown()
{
	delete g_theGame;
	g_theGame = nullptr;

#ifdef ENGINE_VULKAN_RENDERER
	if (g_theDeferred) { g_theDeferred->Shutdown(); delete g_theDeferred; g_theDeferred = nullptr; }
#endif

	g_theEventSystem->Shutdown();
	//g_theAudio->Shutdown();
	g_theRenderer->ShutDown();
	g_theWindow->Shutdown();
	g_theInput->Shutdown();
	g_theDevConsole->Shutdown();

	DebugRenderSystemShutdown();

	delete g_theDevConsole;
	g_theDevConsole = nullptr;

	delete g_theEventSystem;
	g_theEventSystem = nullptr;

	/*delete g_theAudio;
	g_theAudio = nullptr;*/

	delete g_theRenderer;
	g_theRenderer = nullptr;

	delete g_theWindow;
	g_theWindow = nullptr;

	delete g_theInput;
	g_theInput = nullptr;

}

void App::BeginFrame()
{
	Clock::TickSystemClock();
	g_theWindow->BeginFrame();
	g_theInput->BeginFrame();
	g_theRenderer->BeginFrame();
	//g_theAudio->BeginFrame();
	g_theEventSystem->BeginFrame();
	g_theDevConsole->BeginFrame();

	DebugRenderBeginFrame();
}

bool App::IsKeyDown(unsigned char keyCode) const
{
	return g_theInput->IsKeyDown(keyCode);
	//return m_keystates[keyCode].IsPressed();
	//return m_currentKeyStates[keyCode]; // 当前帧按键状态
}

bool App::WasKeyJustPressed(unsigned char keyCode) const
{
	return g_theInput->WasKeyJustPressed(keyCode);
	//return m_keystates[keyCode].WasJustPressed();
	//return m_currentKeyStates[keyCode] && !m_previousKeyStates[keyCode]; // 当前帧按下，上一帧未按下
}

void App::HandleKeyPressed(unsigned char keyCode)
{
	g_theInput->HandleKeyPressed(keyCode);
	//m_keystates[keyCode].UpdateStatus(true);
	//m_currentKeyStates[keyCode] = true;
}

void App::HandleKeyReleased(unsigned char keyCode)
{
	g_theInput->HandleKeyReleased(keyCode);
	//m_keystates[keyCode].UpdateStatus(false);
	m_currentKeyStates[keyCode] = false;
}

bool App::IsKeyReleased(unsigned char keyCode) const
{
	return 	m_currentKeyStates[keyCode];
}

void App::HandleQuitRequested()
{
	g_isQuitting = true;
}

//-----------------------------------------------------------------------------------------------
// One "frame" of the game.  Generally: Input, Update, Render.  We call this 60+ times per second.
// #SD1ToDo: Move this function to Game/App.cpp and rename it to  TheApp::RunFrame()

void App::RunFrame()
{
	//float timeNow = static_cast<float>(GetCurrentTimeSeconds());
	//float deltaSeconds = timeNow - m_timeLastFrameStart;
	////DebuggerPrintf("TimeNow = %.06f\n, TimeNow");
	//m_timeLastFrameStart = timeNow;

	BeginFrame();
	Update();
	Render();
	EndFrame();
}

void App::Update()
{
	if (WasKeyJustPressed(KEYCODE_F8))
	{
		delete g_theGame;
		g_theGame = new Game();
	}

	g_theGame->Update();

	UpdateCursor();
}

void App::Render() const
{
	g_theGame->Render();
}

void App::EndFrame()
{
	// let renderer deal with buffers
	g_theWindow->EndFrame();
	g_theRenderer->EndFrame();
	g_theInput->EndFrame();
	//g_theAudio->EndFrame();
	g_theEventSystem->EndFrame();
	g_theDevConsole->EndFrame();

	for (int i = 0; i < 256; ++i)
	{
		m_keystates[i].EndFrame();
	}

	DebugRenderEndFrame();
}

void App::UpdateCursor()
{
	if (g_theGame->m_isInAttractMode || g_theGame->m_openDevConsole || !g_theWindow->WindowHasFocus())
	{
		g_theInput->SetCursorMode(CursorMode::POINTER);
	}
	else
	{
		g_theInput->SetCursorMode(CursorMode::FPS);
	}
}

bool OnQuitEvent(EventArgs& args)
{
	UNUSED(args);
	g_theApp->HandleQuitRequested();
	return true;
}