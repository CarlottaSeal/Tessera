#include "Player.hpp"
#include "Game/Game.hpp"
#include "Game/Entity.hpp"
#include "Game/App.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Renderer/Camera.hpp"

extern InputSystem* g_theInput;
extern Window* g_theWindow;

Player::Player(Game* owner)
	:Entity(owner)
{
	m_position = Vec3(0.f, 0.f, 0.5f);
	m_angularVelocity = EulerAngles(90.f, 90.f, 90.f);
	m_originYaw = m_angularVelocity.m_yawDegrees;
	m_originPitch = m_angularVelocity.m_pitchDegrees;
	m_originRoll = m_angularVelocity.m_rollDegrees;

	m_velocity = Vec3(2.f, 2.f, 2.f);
	m_originV = m_velocity;
}

Player::~Player()
{
}

void Player::Update(float deltaSeconds)
{
	m_worldCamera.SetPosition(m_position);
	m_worldCamera.SetOrientation(m_orientation);

	XboxController const& controller = g_theInput->GetController(0);
	Vec3 fwd = GetForwardVectorDueToOrientation();
	Vec3 left = GetLeftVectorDueToOrientation();

	IntVec2 windowSize = g_theWindow->GetClientDimensions();
	int mouseDeltaX = g_theInput->m_cursorState.m_cursorClientDelta.x;
	int mouseDeltaY = g_theInput->m_cursorState.m_cursorClientDelta.y;

	m_orientation.m_yawDegrees -= (float)mouseDeltaX * 0.075f;//* deltaSeconds;
	m_orientation.m_pitchDegrees += (float)mouseDeltaY * 0.075f;//deltaSeconds;
	m_orientation.m_yawDegrees += controller.GetRightStickX() * m_angularVelocity.m_yawDegrees * 0.0025f;//deltaSeconds;
	m_orientation.m_pitchDegrees += controller.GetRightStickY() * m_angularVelocity.m_pitchDegrees * 0.0025f;//deltaSeconds;

	if (g_theApp->IsKeyDown('Q') || controller.IsButtonDown(XboxButtonID::LB))
	{
		m_orientation.m_rollDegrees -= m_angularVelocity.m_rollDegrees * deltaSeconds;
	}

	if (g_theApp->IsKeyDown('E') || controller.IsButtonDown(XboxButtonID::RB))
	{
		m_orientation.m_rollDegrees += m_angularVelocity.m_rollDegrees * deltaSeconds;
	}

	if (g_theApp->IsKeyDown('A'))
	{
		m_position += left* m_velocity.y * deltaSeconds;
	}
	if (g_theApp->IsKeyDown('D'))
	{
		m_position -= left * m_velocity.y * deltaSeconds;
	}
	m_position += left * m_velocity.y * controller.GetLeftStickX()*deltaSeconds;

	if (g_theApp->IsKeyDown('W'))
	{
		m_position += fwd * m_velocity.x * deltaSeconds;
	}
	if (g_theApp->IsKeyDown('S'))
	{
		m_position -= fwd * m_velocity.x * deltaSeconds;
	}
	m_position += fwd * m_velocity.x * controller.GetLeftStickY()* deltaSeconds;

	if (g_theApp->IsKeyDown('Z'))// || controller.IsButtonDown(XboxButtonID::LS))
	{
		m_position.z += m_velocity.z * deltaSeconds;
	}
	m_position.z += m_velocity.z * controller.GetLeftTrigger() * deltaSeconds;

	if (g_theApp->IsKeyDown('C'))//|| controller.IsButtonDown(XboxButtonID::RS))
	{
		m_position.z -= m_velocity.z * deltaSeconds;
	}
	m_position.z -= m_velocity.z * controller.GetRightTrigger() * deltaSeconds;

	if (g_theApp->IsKeyDown('H') || controller.IsButtonDown(XboxButtonID::START))
	{
		m_position = Vec3(0.f, 0.f, 0.f);
		m_orientation = EulerAngles(0.f, 0.f, 0.f);
	}

	if (g_theApp->IsKeyDown(KEYCODE_SHIFT) || controller.IsButtonDown(XboxButtonID::A))
	{
		m_velocity = m_originV * 10.f;
		m_angularVelocity.m_yawDegrees = m_originYaw * 10.f;
		m_angularVelocity.m_pitchDegrees = m_originPitch * 10.f;
		m_angularVelocity.m_rollDegrees = m_originRoll * 10.f;
	}
	if (!g_theApp->IsKeyDown(KEYCODE_SHIFT) && !controller.IsButtonDown(XboxButtonID::A))
	{
		m_velocity = m_originV;
		m_angularVelocity.m_yawDegrees   = m_originYaw;
		m_angularVelocity.m_pitchDegrees = m_originPitch;
		m_angularVelocity.m_rollDegrees  = m_originRoll;
	}
	m_orientation.m_yawDegrees = fmodf(m_orientation.m_yawDegrees, 360.0f);
	m_orientation.m_pitchDegrees = GetClamped(m_orientation.m_pitchDegrees, -85.f, 85.f);
	m_orientation.m_rollDegrees = GetClamped(m_orientation.m_rollDegrees, -45.f, 45.f);
}

void Player::Render() const
{
}

Vec3 Player::GetForwardVectorDueToOrientation() const
{
	Vec3 fwd, left, up;
	m_orientation.GetAsVectors_IFwd_JLeft_KUp(fwd, left, up);
	return fwd;
}

Vec3 Player::GetLeftVectorDueToOrientation() const
{
	Vec3 fwd, left, up;
	m_orientation.GetAsVectors_IFwd_JLeft_KUp(fwd, left, up);
	return left;
}
