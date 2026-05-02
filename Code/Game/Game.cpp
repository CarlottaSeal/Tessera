#include "Game.hpp"

// stb_image_write impl lives in DX12Renderer.cpp for DX12 builds; for the Vulkan-only
// build it has to be defined in exactly one TU here so StaticMesh.cpp's stbi_write_png link.
#pragma warning(push)
#pragma warning(disable: 4996)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "ThirdParty/stb/stb_image_write.h"
#pragma warning(pop)

#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Math/AABB2.hpp"    
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "Engine/Core/StaticMesh.h"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"

#include <future>
#include <vector>
#include "Engine/Renderer/VulkanRenderer.h"
#include "Engine/Renderer/VulkanDeferredPath.h"
#include "Engine/Audio/AudioSystem.hpp"

#include "Game/Entity.hpp"
#include "Game/Player.hpp"
#include "Game/Prop.hpp"
#include "Game/Gamecommon.hpp"
#include "Game/EngineBuildPreferences.hpp"

RandomNumberGenerator g_RNG;
//extern AudioSystem* g_theAudio;
extern Clock* s_theSystemClock;

Game::Game()
{
	m_isInAttractMode = true;
	//m_gameCamera.SetOrthoView(Vec2(0.0f, 0.0f), Vec2(200.0f, 100.0f));
	m_screenCamera.SetOrthographicView(Vec2(0.f, 0.f), Vec2(1600.f, 800.f));
	//m_gameCamera.SetOrthographicView(Vec2(-1.0f, -1.0f), Vec2(1.0f, 1.0f));

	m_player = static_cast<Entity*>(new Player(this));
	m_entityList.push_back(m_player);
	m_colorCube = static_cast<Entity*>(new Prop(this, PropType::Cube));
	m_colorCube->m_position = Vec3(2.f, 2.f, 0.f);
	m_entityList.push_back(m_colorCube);
	m_rotateCube = static_cast<Entity*>(new Prop(this, PropType::Cube));
	m_rotateCube->m_position = Vec3(-2.f, -2.f, 0.f);
	m_entityList.push_back(m_rotateCube);
	m_rotateSphere = static_cast<Entity*>(new Prop(this, PropType::Sphere));
	m_rotateSphere->m_position = Vec3(10.f, -5.f, 1.f);
	m_entityList.push_back(m_rotateSphere);

	((Player*)m_player)->m_worldCamera.SetCameraMode(Camera::CameraMode::eMode_Perspective);
	((Player*)m_player)->m_worldCamera.SetPerspectiveView(2.f, 60.f, 0.1f, 100.f);
	Mat44 mat;
	mat.SetIJK3D(Vec3(0.f, 0.f,1.f), Vec3(-1.f, 0.f, 0.f), Vec3(0.f, 1.f, 0.f));
	((Player*)m_player)->m_worldCamera.SetCameraToRenderTransform(mat);
	//m_gameCamera.SetPosition(Vec3(-2.f, 0.f, 0.f));

	//g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	PrintGameControlToDevConsole();
	DrawSquareXYGrid(100);

	DebugAddWorldBasis(mat, -1.f, DebugRenderMode::USE_DEPTH);
	DebugAddWorldAxisText(mat);

	m_chessKing   = new StaticMesh(g_theRenderer, "Data/Models/LewisSet/King_white",   false);
	m_chessQueen  = new StaticMesh(g_theRenderer, "Data/Models/LewisSet/Queen_white",  false);
	m_chessRook   = new StaticMesh(g_theRenderer, "Data/Models/LewisSet/Castle_white",false);
	m_chessBishop = new StaticMesh(g_theRenderer, "Data/Models/LewisSet/Bishop_white",false);
	m_chessKnight = new StaticMesh(g_theRenderer, "Data/Models/LewisSet/Knight_white",false);
	m_chessPawn   = new StaticMesh(g_theRenderer, "Data/Models/LewisSet/Pawn_white",  false);
}

Game::~Game()
{
	for (Entity* entity : m_entityList)
	{
		delete entity;
		entity = nullptr;
	}
	delete m_chessKing;   m_chessKing   = nullptr;
	delete m_chessQueen;  m_chessQueen  = nullptr;
	delete m_chessRook;   m_chessRook   = nullptr;
	delete m_chessBishop; m_chessBishop = nullptr;
	delete m_chessKnight; m_chessKnight = nullptr;
	delete m_chessPawn;   m_chessPawn   = nullptr;
}

void Game::Update()
{
	if (g_theApp->WasKeyJustPressed(KEYCODE_ESC)
		|| g_theInput->GetController(0).WasButtonJustPressed(XboxButtonID::X))
	{
		if (m_isInAttractMode)
		{
			g_theApp->g_isQuitting = true;
		}
		if (!m_isInAttractMode)
		{
			m_isInAttractMode = true;
		}
	}

	AdjustForPauseAndTimeDistortion();

	if (m_isInAttractMode)
	{
		AttractModeUpdate();

		if (!m_hasPlayedAttractSound)
		{
			//SoundID Attract = g_theAudio->CreateOrGetSound("Data/Audio/Attract.MP3");
			//m_attractSoundID = g_theAudio->StartSound(Attract, false, 1.0f, 0.5f, 1.0f, false);
			m_hasPlayedAttractSound = true;
		}
	}

	if (!m_isInAttractMode)
	{
		for (Entity* entity : m_entityList)
		{
			if (entity == m_player)
			{
				entity->Update((float)s_theSystemClock->GetDeltaSeconds());
			}
			if (entity == m_colorCube)
			{
				((Prop*)entity)->UpdateColor((float)m_gameClock->GetDeltaSeconds());
			}
			if (entity == m_rotateCube)
			{
				((Prop*)entity)->UpdateRotate((float)m_gameClock->GetDeltaSeconds());
			}
			if (entity == m_rotateSphere)
			{
				((Prop*)entity)->UpdateRotateForSphere((float)m_gameClock->GetDeltaSeconds());
			}
		}
		DebugRenderSystemInputUpdate();
	}

	if (g_theApp->WasKeyJustPressed(KEYCODE_F2))
	{
		m_useForwardPath = !m_useForwardPath;
	}
	if (g_theApp->WasKeyJustPressed(KEYCODE_F3))
	{
		m_useMultithreading = !m_useMultithreading;
		g_theDeferred->SetMultithreadingEnabled(m_useMultithreading);
	}

	if (g_theDevConsole->GetMode() == OPEN_FULL)
	{
		m_openDevConsole = true;
	}
	if (g_theDevConsole->GetMode() == HIDDEN)
	{
		m_openDevConsole = false;
	}

	m_varyTime += (float)m_gameClock->GetDeltaSeconds();
	if (m_varyTime > 360.f)
	{
		m_varyTime = 0.f;
	}
}

void Game::Render() const
{
	if (m_isInAttractMode)
	{
		g_theRenderer->ClearScreen();
		g_theRenderer->BeginCamera(m_screenCamera);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->SetBlendMode(BlendMode::ALPHA);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
		AttractModeRender();
		g_theRenderer->EndCamera(m_screenCamera);
	}

	if (!m_isInAttractMode)// && !IsInSelectInterface)
	{
#ifdef ENGINE_VULKAN_RENDERER
		VulkanRenderer* vk = g_theRenderer->GetSubRenderer();
		VkCommandBuffer cmd = vk->GetCurrentCommandBuffer();
		uint32_t swapIdx = vk->GetCurrentSwapImageIndex();

		// Build the lights up-front so both deferred and forward paths share the same data.
		const float t = (float)m_gameClock->GetTotalSeconds();
		auto setLightFn = [&](DeferredPointLight& L, Vec3 pos, float radius, Vec3 color, float intensity)
		{
			L.posAndRadius[0] = pos.x; L.posAndRadius[1] = pos.y; L.posAndRadius[2] = pos.z; L.posAndRadius[3] = radius;
			L.colorAndIntensity[0] = color.x; L.colorAndIntensity[1] = color.y; L.colorAndIntensity[2] = color.z; L.colorAndIntensity[3] = intensity;
		};
		DeferredLightConstants lightsAB{};
		lightsAB.numLights = 6;
		const float orbitRAB = 3.5f;
		const float orbitCxAB = 5.f;
		const float orbitCyAB = 0.f;
		setLightFn(lightsAB.lights[0], Vec3(orbitCxAB + cosf(t * 0.6f) * orbitRAB, orbitCyAB + sinf(t * 0.6f) * orbitRAB, 1.5f),
			6.f, Vec3(1.0f, 0.4f, 0.2f), 5.f);
		setLightFn(lightsAB.lights[1], Vec3(orbitCxAB + cosf(t * 0.6f + 2.094f) * orbitRAB, orbitCyAB + sinf(t * 0.6f + 2.094f) * orbitRAB, 1.5f),
			6.f, Vec3(0.2f, 0.6f, 1.0f), 5.f);
		setLightFn(lightsAB.lights[2], Vec3(orbitCxAB + cosf(t * 0.6f + 4.188f) * orbitRAB, orbitCyAB + sinf(t * 0.6f + 4.188f) * orbitRAB, 1.5f),
			6.f, Vec3(0.4f, 1.0f, 0.5f), 5.f);
		setLightFn(lightsAB.lights[3], Vec3(orbitCxAB, orbitCyAB, 4.f + sinf(t * 1.3f) * 0.6f),
			8.f, Vec3(1.0f, 1.0f, 0.9f), 4.f);
		setLightFn(lightsAB.lights[4], Vec3(orbitCxAB + 2.f * cosf(t * 1.1f), orbitCyAB + 4.f, 0.6f),
			5.f, Vec3(1.0f, 0.2f, 0.8f), 4.f);
		setLightFn(lightsAB.lights[5], Vec3(orbitCxAB - 2.f * cosf(t * 1.1f), orbitCyAB - 4.f, 0.6f),
			5.f, Vec3(0.9f, 0.8f, 0.2f), 4.f);

		if (m_useForwardPath)
			g_theDeferred->BeginForwardLit(cmd, swapIdx, lightsAB);
		else
			g_theDeferred->BeginGBuffer(cmd, swapIdx);

		g_theRenderer->BeginCamera(((Player*)m_player)->m_worldCamera);
		g_theRenderer->SetBlendMode(BlendMode::ALPHA);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		for (Entity* entity : m_entityList)
		{
			if (entity != m_player)
			{
				g_theRenderer->SetModelConstants(entity->GetModelToWorldTransform(), ((Prop*)entity)->m_color);
			}
			entity->Render();
		}
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->SetModelConstants();
		g_theRenderer->DrawVertexArray(m_gridVertexes);

		const float row = 5.f;
		const float dy = 1.1f;
		struct PieceSpec { StaticMesh* mesh; Vec3 pos; };
		PieceSpec pieces[16] = {
			{m_chessRook,   Vec3(row, -3.f * dy, 0.f)},
			{m_chessKnight, Vec3(row, -2.f * dy, 0.f)},
			{m_chessBishop, Vec3(row, -1.f * dy, 0.f)},
			{m_chessQueen,  Vec3(row,  0.f * dy, 0.f)},
			{m_chessKing,   Vec3(row,  1.f * dy, 0.f)},
			{m_chessBishop, Vec3(row,  2.f * dy, 0.f)},
			{m_chessKnight, Vec3(row,  3.f * dy, 0.f)},
			{m_chessRook,   Vec3(row,  4.f * dy, 0.f)},
			{m_chessPawn,   Vec3(row - 1.2f, -3.5f * dy, 0.f)},
			{m_chessPawn,   Vec3(row - 1.2f, -2.5f * dy, 0.f)},
			{m_chessPawn,   Vec3(row - 1.2f, -1.5f * dy, 0.f)},
			{m_chessPawn,   Vec3(row - 1.2f, -0.5f * dy, 0.f)},
			{m_chessPawn,   Vec3(row - 1.2f,  0.5f * dy, 0.f)},
			{m_chessPawn,   Vec3(row - 1.2f,  1.5f * dy, 0.f)},
			{m_chessPawn,   Vec3(row - 1.2f,  2.5f * dy, 0.f)},
			{m_chessPawn,   Vec3(row - 1.2f,  3.5f * dy, 0.f)},
		};

		std::future<void> workerA, workerB;
		if (m_useMultithreading)
		{
			// Pre-stage on main: capture per-piece state into PreparedDraws. SetModelConstants
			// advances the model UBO ring; SetMaterialConstants writes the bindless slot trio.
			static std::vector<VulkanDeferredPath::PreparedDraw> prepared;
			prepared.clear();
			prepared.reserve(16);
			VkPipeline pcuTbnPipe = g_theDeferred->GetGBufferPCUTBNPipeline();
			for (auto const& p : pieces)
			{
				if (!p.mesh || !p.mesh->m_vertexBuffer || !p.mesh->m_indexBuffer) continue;
				Mat44 model;
				model.SetTranslation3D(p.pos);
				model.Append(p.mesh->m_transform);
				g_theRenderer->SetMaterialConstants(p.mesh->m_diffuseTexture, p.mesh->m_normalTexture, p.mesh->m_specularTexture);
				g_theRenderer->SetModelConstants(model, Rgba8::WHITE);

				VulkanDeferredPath::PreparedDraw d;
				d.pipeline            = pcuTbnPipe;
				d.vbo                 = p.mesh->m_vertexBuffer->GetVkBuffer();
				d.ibo                 = p.mesh->m_indexBuffer->GetVkBuffer();
				d.indexCount          = (unsigned int)p.mesh->m_indices.size();
				d.cameraDynamicOffset = vk->GetCurrentCameraDynamicOffset();
				d.modelDynamicOffset  = vk->GetCurrentModelDynamicOffset();
				d.material            = vk->GetCurrentMaterial();
				prepared.push_back(d);
			}

			// Kick workers (don't wait yet — main continues with debug-render in parallel).
			const int half = (int)prepared.size() / 2;
			workerA = std::async(std::launch::async, [&]() {
				VkCommandBuffer sec = g_theDeferred->BeginThreadSecondary(1, swapIdx);
				g_theDeferred->RecordPreparedDraws(sec, prepared.data(), half);
				g_theDeferred->EndThreadSecondary(1);
			});
			workerB = std::async(std::launch::async, [&]() {
				VkCommandBuffer sec = g_theDeferred->BeginThreadSecondary(2, swapIdx);
				g_theDeferred->RecordPreparedDraws(sec, prepared.data() + half, (int)prepared.size() - half);
				g_theDeferred->EndThreadSecondary(2);
			});
		}
		else
		{
			auto drawPiece = [](StaticMesh* mesh, Vec3 const& pos, float yawDeg, Rgba8 tint)
			{
				if (!mesh || !mesh->m_vertexBuffer || !mesh->m_indexBuffer) return;
				Mat44 model;
				model.AppendZRotation(yawDeg);
				model.SetTranslation3D(pos);
				model.Append(mesh->m_transform);
				g_theRenderer->SetMaterialConstants(mesh->m_diffuseTexture, mesh->m_normalTexture, mesh->m_specularTexture);
				g_theRenderer->SetModelConstants(model, tint);
				g_theRenderer->DrawIndexBuffer(mesh->m_vertexBuffer, mesh->m_indexBuffer,
					(unsigned int)mesh->m_indices.size());
			};
			for (auto const& p : pieces)
				drawPiece(p.mesh, p.pos, 0.f, Rgba8::WHITE);
		}

		for (int i = 0; i < lightsAB.numLights; ++i)
		{
			Vec3 c(lightsAB.lights[i].posAndRadius[0], lightsAB.lights[i].posAndRadius[1], lightsAB.lights[i].posAndRadius[2]);
			Rgba8 tint(
				(unsigned char)(lightsAB.lights[i].colorAndIntensity[0] * 255.f),
				(unsigned char)(lightsAB.lights[i].colorAndIntensity[1] * 255.f),
				(unsigned char)(lightsAB.lights[i].colorAndIntensity[2] * 255.f),
				255);
			DebugAddWorldPoint(c, 0.15f, 0.f, tint, tint, DebugRenderMode::USE_DEPTH);
		}
		DebugRenderWorld(((Player*)m_player)->m_worldCamera);

		// If MT was kicked, wait for both workers before EndGBufferAndRunLighting collects
		// secondaries for vkCmdExecuteCommands.
		if (workerA.valid()) workerA.wait();
		if (workerB.valid()) workerB.wait();

		if (m_useForwardPath)
			g_theDeferred->EndForwardLit(cmd);
		else
			g_theDeferred->EndGBufferAndRunLighting(cmd, lightsAB);

		char modeBuf[64];
		snprintf(modeBuf, sizeof(modeBuf), " mode: %s [F2]   MT: %s [F3]",
			m_useForwardPath ? "FORWARD" : "DEFERRED",
			m_useMultithreading ? "ON" : "OFF");
		DebugAddScreenText(modeBuf, m_screenCamera.GetOrthographicTopRight() - Vec2(360.f, 50.f), 12.f, Vec2::ZERO, 0.f);

		double gbufMs = 0.0, lightMs = 0.0, fwdMs = 0.0;
		if (g_theDeferred->TryGetLastFrameTimings(gbufMs, lightMs))
		{
			char buf[128];
			snprintf(buf, sizeof(buf), " deferred: gbuf %.2f ms  lighting %.2f ms  total %.2f ms",
				gbufMs, lightMs, gbufMs + lightMs);
			DebugAddScreenText(buf, m_screenCamera.GetOrthographicTopRight() - Vec2(500.f, 30.f), 12.f, Vec2::ZERO, 0.f);
		}
		if (g_theDeferred->TryGetLastForwardMs(fwdMs))
		{
			char buf[128];
			snprintf(buf, sizeof(buf), " forward total: %.2f ms", fwdMs);
			DebugAddScreenText(buf, m_screenCamera.GetOrthographicTopRight() - Vec2(500.f, 70.f), 12.f, Vec2::ZERO, 0.f);
		}

		g_theDeferred->BeginForwardOverlay(cmd, swapIdx);
		DebugRenderScreen(m_screenCamera);
		g_theDevConsole->Render(AABB2(m_screenCamera.GetOrthographicBottomLeft(), m_screenCamera.GetOrthographicTopRight()), g_theRenderer);
		g_theDeferred->EndForwardOverlay(cmd);
#else
		g_theRenderer->ClearScreen(Rgba8(0, 0, 0));
		g_theRenderer->BeginCamera(((Player*)m_player)->m_worldCamera);
		g_theRenderer->SetBlendMode(BlendMode::ALPHA);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		for (Entity* entity : m_entityList)
		{
			if (entity != m_player)
			{
				g_theRenderer->SetModelConstants(entity->GetModelToWorldTransform(), ((Prop*)entity)->m_color);
			}
			entity->Render();
		}
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->SetModelConstants();
		g_theRenderer->DrawVertexArray(m_gridVertexes);
		g_theRenderer->EndCamera(((Player*)m_player)->m_worldCamera);

		DebugRenderWorld(((Player*)m_player)->m_worldCamera);
		DebugRenderScreen(m_screenCamera);
#endif
	}

	/*if (m_openDevConsole)
	{*/
		/*if (m_isInAttractMode)
		{
			g_theRenderer->BeginCamera(m_screenCamera);*/
#ifndef ENGINE_VULKAN_RENDERER
		g_theDevConsole->Render(AABB2(m_screenCamera.GetOrthographicBottomLeft(), m_screenCamera.GetOrthographicTopRight()), g_theRenderer);
#else
		// 3D Vulkan path renders DevConsole inside the overlay pass (see above).
		if (m_isInAttractMode)
			g_theDevConsole->Render(AABB2(m_screenCamera.GetOrthographicBottomLeft(), m_screenCamera.GetOrthographicTopRight()), g_theRenderer);
#endif
		//	//g_theRenderer->EndCamera(m_screenCamera);
		//}
		//if (!m_isInAttractMode)
		//{
		//	//g_theRenderer->BeginCamera(m_gameCamera);
		//	g_theDevConsole->Render(AABB2(m_screenCamera.GetOrthographicBottomLeft(), m_screenCamera.GetOrthographicTopRight()), g_theRenderer);
		//	//g_theRenderer->EndCamera(m_gameCamera);
		//}
	//}
}

void Game::AdjustForPauseAndTimeDistortion()
{
	if (g_theApp->WasKeyJustPressed('T'))
	{
		m_isSlowMo = !m_isSlowMo;
		m_isUsingUserTimeScale = false;
	}
	if (m_isUsingUserTimeScale) // 如果用户通过 set_time_scale 设定了时间缩放，保持该值
	{
		m_gameClock->SetTimeScale(m_userTimeScale);
	}
	else
	{
		m_gameClock->SetTimeScale(m_isSlowMo ? 0.1f : 1.0f);
	}

	if (g_theApp->WasKeyJustPressed('P'))
	{
		m_gameClock->TogglePause();
		if (!m_gameClock->IsPaused())
		{
			m_gameClock->Unpause();
			m_gameClock->Reset();
			//SoundID Pause = g_theAudio->CreateOrGetSound("Data/Audio/Pause.mp3");
			//g_theAudio->StartSound(Pause, false, 0.05f, 0.5f, 1.f, false);
		}
		if (m_gameClock->IsPaused())
		{
			m_gameClock->Reset();
			//SoundID Unpause = g_theAudio->CreateOrGetSound("Data/Audio/Unpause.mp3");
			//g_theAudio->StartSound(Unpause, false, 0.05f, 0.5f, 1.f, false);
		}
	}

	if (g_theApp->WasKeyJustPressed('O'))
	{
		m_gameClock->StepSingleFrame();
	}
}

void Game::AttractModeUpdate()
{
	if (g_theApp->IsKeyDown(' ') ||
		g_theApp->IsKeyDown('N') ||
		g_theInput->GetController(0).WasButtonJustPressed(XboxButtonID::START) ||
		g_theInput->GetController(0).WasButtonJustPressed(XboxButtonID::A))
	{
		m_isInAttractMode = false;
	}
}

void Game::AttractModeRender() const
{
	DebugDrawRing(Vec2(800.f, 400.f), 100.f, 10.f + sinf(m_varyTime) * 10.f, Rgba8::YELLOW);
}

void Game::PrintGameControlToDevConsole()
{
	g_theDevConsole->AddLine(Rgba8::BLUE, "Type help for a list of commands");
	g_theDevConsole->AddLine(Rgba8::CYAN, "Mouse x-axis Right stick x-axis Yaw");
	g_theDevConsole->AddLine(Rgba8::CYAN, "Mouse y-axis Right stick y-axis Pitch");
	g_theDevConsole->AddLine(Rgba8::CYAN, "Q / E Left trigger / right trigger Roll");
	g_theDevConsole->AddLine(Rgba8::CYAN, "A / D Left stick x-axis Move left or right");
	g_theDevConsole->AddLine(Rgba8::CYAN, "W / S Left stick y-axis Move forward or back");
	g_theDevConsole->AddLine(Rgba8::CYAN, "Z / C Left shoulder / right shoulder Move down or up");
	g_theDevConsole->AddLine(Rgba8::CYAN, "H / Start button Reset position and orientation to zero");
	g_theDevConsole->AddLine(Rgba8::CYAN, "Shift / A button Increase speed by a factor of 10 while held");
	g_theDevConsole->AddLine(Rgba8::CYAN, "P - Pause the game");
	g_theDevConsole->AddLine(Rgba8::CYAN, "O - Single step frame");
	g_theDevConsole->AddLine(Rgba8::CYAN, "T - Slow motion mode");
	g_theDevConsole->AddLine(Rgba8::CYAN, "1 - Spawn line");
	g_theDevConsole->AddLine(Rgba8::CYAN, "2 - Spawn point");
	g_theDevConsole->AddLine(Rgba8::CYAN, "3 - Spawn wireframe sphere");
	g_theDevConsole->AddLine(Rgba8::CYAN, "4 - Spawn basis");
	g_theDevConsole->AddLine(Rgba8::CYAN, "5 - Spawn billboard");
	g_theDevConsole->AddLine(Rgba8::CYAN, "6 - Spawn wireframe cylinder");
	g_theDevConsole->AddLine(Rgba8::CYAN, "7 - Add message");
	g_theDevConsole->AddLine(Rgba8::CYAN, "ESC - Exit game");
}

void Game::DrawSquareXYGrid(int unit /*= 100*/)
{
	m_gridVertexes.clear();

	const int GRID_SIZE = unit * unit;
	const float LINE_THICKNESS = 0.03f;
	const float BOLD_LINE_THICKNESS = 0.05f;
	const float ORIGIN_LINE_THICKNESS = 0.1f;

	for (int x = -GRID_SIZE / 2; x <= GRID_SIZE / 2; ++x)
	{
		float thickness = LINE_THICKNESS;
		Rgba8 color = Rgba8::GREY; 

		if (x % 5 == 0)
		{
			thickness = BOLD_LINE_THICKNESS;
			color = Rgba8::RED; 
		}
		if (x == 0)
		{
			thickness = ORIGIN_LINE_THICKNESS;
			color = Rgba8(255, 50, 50, 255);
		}

		AABB3 bounds(Vec3(-GRID_SIZE / 2.f, (float)x - thickness / 2.f, -thickness / 2.f),
			Vec3(GRID_SIZE / 2.f, (float)x + thickness / 2.f, thickness / 2.f));

		AddVertsForAABB3D(m_gridVertexes, bounds, color, AABB2::ZERO_TO_ONE);
	}

	for (int y = -GRID_SIZE / 2; y <= GRID_SIZE / 2; ++y)
	{
		float thickness = LINE_THICKNESS * 1.1f;
		Rgba8 color = Rgba8::GREY;

		if (y % 5 == 0)
		{
			thickness = BOLD_LINE_THICKNESS * 1.1f;
			color = Rgba8::GREEN; 
		}
		if (y == 0)
		{
			thickness = ORIGIN_LINE_THICKNESS * 1.1f;
			color = Rgba8(50, 255, 50, 255);
		}

		AABB3 bounds(Vec3((float)y - thickness / 2.f, -GRID_SIZE / 2.f , -thickness/2.f),
			Vec3((float)y + thickness / 2.f, GRID_SIZE / 2.f, thickness/2.f));

		AddVertsForAABB3D(m_gridVertexes, bounds, color, AABB2::ZERO_TO_ONE);
	}
}

void Game::DebugRenderSystemInputUpdate()
{
	Vec3 pos = m_player->m_position;
	std::string reportHUD = " Player Position: " +
		RoundToOneDecimalString(pos.x) + ", " + RoundToOneDecimalString(pos.y) + ", " + RoundToOneDecimalString(pos.z);
	DebugAddMessage(reportHUD, 0.f, m_screenCamera);

	if (g_theApp->WasKeyJustPressed('1'))
	{
		Vec3 playerI = m_player->GetModelToWorldTransform().GetIBasis3D();
		Vec3 end = pos + playerI * 20.f;
		DebugAddWorldLine(pos, end, 0.1f, 10.f, Rgba8::YELLOW, Rgba8::YELLOW, DebugRenderMode::X_RAY);
	}
	if (g_theApp->IsKeyDown('2'))
	{
		DebugAddWorldPoint(Vec3(pos.x,pos.y,0.f), 0.2f, 60.f, Rgba8(150, 75, 0), Rgba8(150, 75, 0), DebugRenderMode::USE_DEPTH);
	}
	if (g_theApp->WasKeyJustPressed('3'))
	{
		Vec3 playerI = m_player->GetModelToWorldTransform().GetIBasis3D();
		Vec3 center = pos + playerI * 2.f;
		DebugAddWorldWireSphere(center, 1.f, 5.f, Rgba8::GREEN, Rgba8::RED);//, DebugRenderMode::USE_DEPTH);
	}
	if (g_theApp->WasKeyJustPressed('4'))
	{
		Mat44 playerMat = m_player->GetModelToWorldTransform();
		Vec3 playerI = m_player->GetModelToWorldTransform().GetIBasis3D().GetNormalized();
		Vec3 playerJ = m_player->GetModelToWorldTransform().GetJBasis3D().GetNormalized();
		Vec3 playerK = m_player->GetModelToWorldTransform().GetKBasis3D().GetNormalized();
		playerMat.SetIJK3D(playerI, playerJ, playerK);
		/*Mat44 mat;
		mat.SetIJK3D(Vec3(0.f, -1.f, 0.f), Vec3(0.f, 0.f, 1.f), Vec3(1.f, 0.f, 0.f));
		playerMat.Append(mat);*/
		//DebugAddWorldBasis(playerMat, 20.f);//, DebugRenderMode::USE_DEPTH);
		Vec3 newK = pos + playerK;
		Vec3 newI = pos + playerI;
		Vec3 newJ = pos + playerJ;
		DebugAddWorldArrow(pos, newK, 0.05f, 20.f, Rgba8::AQUA, Rgba8::AQUA);
		DebugAddWorldArrow(pos, newI, 0.05f, 20.f, Rgba8::MAGENTA, Rgba8::MAGENTA);
		DebugAddWorldArrow(pos, newJ, 0.05f, 20.f, Rgba8::MINTGREEN, Rgba8::MINTGREEN);
	}
	if (g_theApp->WasKeyJustPressed('5'))
	{
		EulerAngles ori = m_player->m_orientation;
		Vec3 playerI = m_player->GetModelToWorldTransform().GetIBasis3D().GetNormalized();
		Vec3 pivot = pos + playerI * 2.f;
		/*std::string report = "Position: " +
			std::to_string(RoundToOneDecimal(pos.x)) + ", " + std::to_string(RoundToOneDecimal(pos.y)) + ", " + std::to_string(RoundToOneDecimal(pos.z)) +
			" Orientation: " + std::to_string(RoundToOneDecimal(ori.m_yawDegrees)) + ", " + std::to_string(RoundToOneDecimal(ori.m_pitchDegrees)) + ", "
			+ std::to_string(RoundToOneDecimal(ori.m_rollDegrees));*/
		std::string report = "Position: " +
			RoundToOneDecimalString(pos.x) + ", " + RoundToOneDecimalString(pos.y) + ", " + RoundToOneDecimalString(pos.z) +
			" Orientation: " + RoundToOneDecimalString(ori.m_yawDegrees) + ", " + RoundToOneDecimalString(ori.m_pitchDegrees) + ", "
			+ RoundToOneDecimalString(ori.m_rollDegrees);

		DebugAddWorldBillboardText(report, pivot, 0.2f, Vec2(0.25f, 0.25f), 10.f, Rgba8::WHITE);// , DebugRenderMode::USE_DEPTH);
	}
	if (g_theApp->WasKeyJustPressed('6'))
	{
		DebugAddWorldWireCylinder(pos, pos + Vec3(0.f, 0.f, 2.f), 0.5f, 10.f, Rgba8::WHITE, Rgba8::RED);
	}
	if (g_theApp->WasKeyJustPressed('7'))
	{
		EulerAngles ori = m_player->m_orientation;
		std::string report = " Camera Orientation: " + RoundToOneDecimalString(ori.m_yawDegrees) + ", " + RoundToOneDecimalString(ori.m_pitchDegrees) + ", "
			+ RoundToOneDecimalString(ori.m_rollDegrees);
		DebugAddMessage(report, 5.f, m_screenCamera);
	}

	//Add fps...
	float timeTotal = (float)m_gameClock->GetTotalSeconds();
	float fps = (float)m_gameClock->GetFrameRate();
	float timeScale = m_gameClock->GetTimeScale();
	std::string timeReportHUD = " Time: " + RoundToTwoDecimalsString(timeTotal) 
	+ " FPS: " + RoundToOneDecimalString(fps) + " Scale: " + RoundToTwoDecimalsString(timeScale);
	float textWidth = GetTextWidth(12.f, timeReportHUD, 0.7f);
	DebugAddScreenText(timeReportHUD, m_screenCamera.GetOrthographicTopRight() - Vec2(textWidth + 1.f, 15.f), 12.f, Vec2::ZERO, 0.f);
}

void Game::DebugAddWorldAxisText(Mat44 worldMat)
{
	Mat44 xMat;
	xMat.SetIJK3D(Vec3(-1.f, 0.f, 0.f), Vec3(0.f, 0.f, 1.f), Vec3(0.f, 1.f, 0.f));
	xMat.Append(worldMat);
	DebugAddWorldText("x-Forward", xMat, 0.2f, Vec2(-0.05f, -0.3f), -1.f, Rgba8::MAGENTA);

	Mat44 yMat;
	yMat.SetIJK3D(Vec3(0.f, 1.f, 0.f), Vec3(0.f, 0.f, 1.f), Vec3(1.f, 0.f, 0.f));
	yMat.Append(worldMat);
	DebugAddWorldText("y-Left", yMat, 0.2f, Vec2( 1.f,-0.3f), -1.f, Rgba8::MINTGREEN);

	Mat44 zMat;
	zMat.SetIJK3D(Vec3(0.f, 0.f, -1.f), Vec3(0.f, 1.f, 0.f), Vec3(1.f, 0.f, 0.f));
	zMat.Append(worldMat);
	DebugAddWorldText("z-Up", zMat, 0.2f, Vec2(-0.3f, 1.5f), -1.f, Rgba8::AQUA);
}
