#include "Prop.hpp"
#include "Game/Game.hpp"
#include "Game/Entity.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Core/VertexUtils.hpp"

Prop::Prop(Game* owner, PropType propType)
	:Entity(owner)
{
	m_type = propType;
	InitializeVertsAndTexture();
}

Prop::~Prop()
{
}

void Prop::Update(float deltaSeconds)
{
	UNUSED(deltaSeconds);
	//m_orientation.m_yawDegrees += m_angularVelocity.m_yawDegrees * deltaSeconds;
	//m_orientation.m_pitchDegrees += m_angularVelocity.m_pitchDegrees * deltaSeconds;
	//m_orientation.m_rollDegrees += m_angularVelocity.m_rollDegrees * deltaSeconds;
}

void Prop::UpdateColor(float deltaSeconds)
{
	static float timeElapsed = 0.0f;
	timeElapsed += deltaSeconds;

	float brightness = 127.5f + sinf(timeElapsed) * 127.5f;

	m_color = Rgba8(static_cast<unsigned char>(brightness),
		static_cast<unsigned char>(brightness),
		static_cast<unsigned char>(brightness));
}

void Prop::UpdateRotate(float deltaSeconds)
{
	m_orientation.m_yawDegrees += 30.f * deltaSeconds;
	m_orientation.m_pitchDegrees += 30.f * deltaSeconds;
	m_orientation.m_rollDegrees += 30.f * deltaSeconds;
	m_orientation.m_yawDegrees = fmodf(m_orientation.m_yawDegrees, 360.0f);
	m_orientation.m_pitchDegrees = fmodf(m_orientation.m_rollDegrees, 360.0f);
	m_orientation.m_rollDegrees = fmodf(m_orientation.m_rollDegrees, 360.0f);
}

void Prop::UpdateRotateForSphere(float deltaSeconds)
{
	m_orientation.m_yawDegrees += 45.f * deltaSeconds;
	m_orientation.m_yawDegrees = fmodf(m_orientation.m_yawDegrees, 360.0f);
}

void Prop::Render() const
{
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->BindTexture(m_texture);
	//TransformVertexArray3D(m_vertexes.size(),
	g_theRenderer->DrawVertexArray(m_vertexes);
}

void Prop::InitializeVertsAndTexture()
{
	if(m_type == PropType::Cube)
	{
		//+x
		AddVertsForQuad3D(m_vertexes, Vec3(0.5f, -0.5f, -0.5f), Vec3(0.5f, 0.5f, -0.5f),
			Vec3(0.5f, 0.5f, 0.5f), Vec3(0.5f, -0.5f, 0.5f), Rgba8::RED);
		//-x
		AddVertsForQuad3D(m_vertexes, Vec3(-0.5f, 0.5f, -0.5f), Vec3(-0.5f, -0.5f, -0.5f),
			Vec3(-0.5f, -0.5f, 0.5f), Vec3(-0.5f, 0.5f, 0.5f), Rgba8::CYAN);
		//+y
		AddVertsForQuad3D(m_vertexes, Vec3(0.5f, 0.5f, -0.5f), Vec3(-0.5f, 0.5f, -0.5f),
			Vec3(-0.5f, 0.5f, 0.5f), Vec3(0.5f, 0.5f, 0.5f), Rgba8::GREEN);
		//-y
		AddVertsForQuad3D(m_vertexes, Vec3(-0.5f, -0.5f, -0.5f), Vec3(0.5f, -0.5f, -0.5f),
			Vec3(0.5f, -0.5f, 0.5f), Vec3(-0.5f, -0.5f, 0.5f), Rgba8::MAGENTA);
		//+z
		AddVertsForQuad3D(m_vertexes, Vec3(-0.5f, 0.5f, 0.5f), Vec3(-0.5f, -0.5f, 0.5f),
			Vec3(0.5f, -0.5f, 0.5f), Vec3(0.5f, 0.5f, 0.5f), Rgba8::BLUE);
		//-z
		AddVertsForQuad3D(m_vertexes, Vec3(0.5f, 0.5f, -0.5f), Vec3(0.5f, -0.5f, -0.5f),
			Vec3(-0.5f, -0.5f, -0.5f), Vec3(-0.5f, 0.5f, -0.5f), Rgba8::YELLOW);
	}
	if (m_type == PropType::Sphere)
	{
		AddVertsForSphere3D(m_vertexes, m_position, 1.f, Rgba8::WHITE, AABB2::ZERO_TO_ONE, 16, 8);
		m_texture = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/TestUV.png");
	}
}
