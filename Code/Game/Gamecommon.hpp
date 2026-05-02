#pragma once
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/EngineCommon.hpp"

void DebugDrawRing( Vec2 const& center, float radius, float thickness, Rgba8 const& color );
void DebugDrawLine( Vec2 const& start, Vec2 const& end, Rgba8 color, float thickness );

constexpr float PI = 3.1415926535897932384626433832795f;

constexpr int NUM_LINE_TRIS = 2;
constexpr int NUM_LINE_VERTS = 3 * NUM_LINE_TRIS;

constexpr int NUM_SIDES = 32;  
constexpr int NUM_TRIS = NUM_SIDES;  
constexpr int NUM_VERTS = 3 * NUM_TRIS; 

class App;
extern App* g_theApp;

class Game;
extern Game* m_game;

class InputSystem;

extern InputSystem* g_theInput;

class Renderer;

extern Renderer* g_theRenderer;

class VulkanDeferredPath;
extern VulkanDeferredPath* g_theDeferred;

