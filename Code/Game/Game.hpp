#pragma once
#include "Game/Gamecommon.hpp"
#include "Game/App.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Audio/AudioSystem.hpp"

#include <vector>

class Clock;
class Entity;
class StaticMesh;

class Game 
{
public:
	Game();
	~Game();

	void Update();
	void Render() const;

	void AdjustForPauseAndTimeDistortion();

public:
	bool m_openDevConsole = false;
	bool m_isInAttractMode;
	Clock* m_gameClock;
	//Camera m_gameCamera;
	Camera m_screenCamera;

	Entity* m_player;
	Entity* m_colorCube;
	Entity* m_rotateCube;
	Entity* m_rotateSphere;
	std::vector<Entity*> m_entityList;

	StaticMesh* m_chessKing   = nullptr;
	StaticMesh* m_chessQueen  = nullptr;
	StaticMesh* m_chessRook   = nullptr;
	StaticMesh* m_chessBishop = nullptr;
	StaticMesh* m_chessKnight = nullptr;
	StaticMesh* m_chessPawn   = nullptr;

private:
	void AttractModeUpdate();
	void AttractModeRender() const;
	//void UpdateCamera();  //move into m_player
	void PrintGameControlToDevConsole();
	void DrawSquareXYGrid(int unit = 100);
	void DebugRenderSystemInputUpdate();
	void DebugAddWorldAxisText(Mat44 worldMat);

private:
	bool m_useForwardPath = false; // F2 toggles between deferred (default) and forward A/B paths
	bool m_isSlowMo;
	bool m_isUsingUserTimeScale;

	float m_userTimeScale;

	bool m_hasPlayedAttractSound = false;
	SoundPlaybackID m_attractSoundID = MISSING_SOUND_ID;

	float m_varyTime = 0.f;

	std::vector<Vertex_PCU> m_gridVertexes;
};




