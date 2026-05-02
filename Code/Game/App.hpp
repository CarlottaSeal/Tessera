#pragma once
#include "Engine/Math/Vec2.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Game/Game.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Input/KeyButtonState.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Audio/AudioSystem.hpp"

class Game;

class App
{
public:
	App();
	~App();

	void Startup();
	void Shutdown();
	void RunFrame();

	bool IsQuitting() const { return g_isQuitting; }
	void HandleKeyPressed(unsigned char keyCode);
	void HandleKeyReleased(unsigned char keycode);
	//void WmMessageCoded();
	void HandleQuitRequested();

	bool IsKeyDown(unsigned char keyCode) const;         
	bool WasKeyJustPressed(unsigned char keyCode) const; 
	bool IsKeyReleased(unsigned char keyCode) const;

	//void AttractModeUpdate(float deltaSeconds);
	//void AttractModeRender() const;

public:
	bool g_isQuitting = false;

private:
	void BeginFrame();
	void Update();
	void Render() const;
	void EndFrame();

	void UpdateCursor();
	
private:
	bool m_currentKeyStates[256] = { false };  // 当前帧的按键状态
	bool m_previousKeyStates[256] = { false }; // 上一帧的按键状态

	KeyButtonState m_keystates[NUM_KEYCODES];

	//bool g_isPaused = false;
	//bool g_isSlowMo = false;
	//bool g_PauseAfterUpdate = false;

	/*bool m_openDevConsole = false;

	bool m_hasPlayedAttractSound = false;
	SoundPlaybackID m_attractSoundID = MISSING_SOUND_ID*/;

	//Camera m_gameCamera;
	//Camera m_screenCamera;
};

bool OnQuitEvent(EventArgs& args);
