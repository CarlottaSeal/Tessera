#include "Game/GameCommon.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "math.h"
#include "Game/EngineBuildPreferences.hpp"



void DebugDrawRing(Vec2 const& center, float radius, float thickness, Rgba8 const& color)
{
	float halfThickness = 0.5f * thickness;
	float innerRadius = radius - halfThickness;
	float outerRadius = radius + halfThickness;

	constexpr int NUM_OF_SIDES = 32;
	constexpr int NUM_OF_TRIS = NUM_OF_SIDES * 2;
	constexpr int NUM_OF_VERTS = 3 * NUM_OF_TRIS;

	Vertex_PCU verts[NUM_OF_VERTS];
	float DEGREES_PER_SIDE = 360.f / static_cast<float>(NUM_OF_SIDES);
	for (int sideNum = 0; sideNum < NUM_OF_SIDES; ++sideNum)
	{
		float startDegrees = DEGREES_PER_SIDE * static_cast<float>(sideNum);
		float endDegrees = DEGREES_PER_SIDE * static_cast<float>(sideNum + 1);
		float cosStart = cosf(startDegrees * PI / 180.f);
		float sinStart = sinf(startDegrees * PI / 180.f);
		float cosEnd = cosf(endDegrees * PI / 180.f);
		float sinEnd = sinf(endDegrees * PI / 180.f);

		Vec3 innerStartPos(center.x + innerRadius * cosStart, center.y + innerRadius * sinStart, 0.f);
		Vec3 outerStartPos(center.x + outerRadius * cosStart, center.y + outerRadius * sinStart, 0.f);
		Vec3 innerEndPos(center.x + innerRadius * cosEnd, center.y + innerRadius * sinEnd, 0.f);
		Vec3 outerEndPos(center.x + outerRadius * cosEnd, center.y + outerRadius * sinEnd, 0.f);

		int vertIndexA = 6 * sideNum;
		int vertIndexB = vertIndexA + 1;
		int vertIndexC = vertIndexA + 2;
		int vertIndexD = vertIndexA + 3;
		int vertIndexE = vertIndexA + 4;
		int vertIndexF = vertIndexA + 5;

		// **修正顶点顺序，使其按照逆时针排列**
		verts[vertIndexA].m_position = innerStartPos;  // 原: innerEndPos
		verts[vertIndexB].m_position = outerStartPos;  // 原: innerStartPos
		verts[vertIndexC].m_position = innerEndPos;    // 原: outerStartPos

		verts[vertIndexA].m_color = color;
		verts[vertIndexB].m_color = color;
		verts[vertIndexC].m_color = color;

		verts[vertIndexD].m_position = outerStartPos;  // 原: innerEndPos
		verts[vertIndexE].m_position = outerEndPos;    // 原: outerEndPos
		verts[vertIndexF].m_position = innerEndPos;    // 原: outerStartPos

		verts[vertIndexD].m_color = color;
		verts[vertIndexE].m_color = color;
		verts[vertIndexF].m_color = color;
	}

	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->DrawVertexArray(NUM_OF_VERTS, &verts[0]);
}


void DebugDrawLine(Vec2 const& start, Vec2 const& end, Rgba8 color, float thickness)
{
    Vertex_PCU lineVerts[NUM_LINE_VERTS];
    Vec2 SE = end - start;
    float h = thickness * 0.5f;
    Vec2 se = SE.GetNormalized();
    Vec2 fwd = se * h;
    Vec2 left = fwd.GetRotated90Degrees();
    Vec2 EL = end + fwd + left;
    Vec2 ER = end + fwd - left;
    Vec2 SL = start - fwd + left;
    Vec2 SR = start - fwd - left;

    lineVerts[0] = Vertex_PCU(Vec3(EL.x, EL.y, 0.f), color, Vec2(0.5f, 0.5f));
    lineVerts[1] = Vertex_PCU(Vec3(ER.x, ER.y, 0.f), color, Vec2(0.5f, 0.5f));
    lineVerts[2] = Vertex_PCU(Vec3(SR.x, SR.y, 0.f), color, Vec2(0.5f, 0.5f));

    lineVerts[3] = Vertex_PCU(Vec3(SL.x, SL.y, 0.f), color, Vec2(0.5f, 0.5f));
    lineVerts[4] = Vertex_PCU(Vec3(SR.x, SR.y, 0.f), color, Vec2(0.5f, 0.5f));
    lineVerts[5] = Vertex_PCU(Vec3(EL.x, EL.y, 0.f), color, Vec2(0.5f, 0.5f));


    g_theRenderer->DrawVertexArray(NUM_LINE_VERTS, &lineVerts[0]);
}

