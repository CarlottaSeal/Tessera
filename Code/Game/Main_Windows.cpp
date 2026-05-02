#define WIN32_LEAN_AND_MEAN		// Always #define this before #including <windows.h>
#include <windows.h>			// #include this (massive, platform-specific) header in VERY few places (and .CPPs only)
#include <math.h>
#include <cassert>
#include <crtdbg.h>
#include "Game/App.hpp"
#include "Game/EngineBuildPreferences.hpp"
#include "Game/Gamecommon.hpp"

#define UNUSED(x) (void)(x);

constexpr float CLIENT_ASPECT = 2.0f; // We are requesting a 2:1 aspect (square) window area



int WINAPI WinMain(HINSTANCE applicationInstanceHandle, HINSTANCE, LPSTR commandLineString, int)
{
	UNUSED(commandLineString);
	UNUSED(applicationInstanceHandle);
	g_theApp = new App();// #SD1ToDo: g_theApp = new App();
	g_theApp->Startup();
	
	while (!g_theApp->IsQuitting())			// #SD1ToDo: ...becomes:  !g_theApp->IsQuitting()
	{
		g_theApp->RunFrame(); // #SD1ToDo: g_theApp->RunFrame();
	}
	g_theApp->Shutdown();
	delete g_theApp;
	g_theApp = nullptr;

	return 0;
}
