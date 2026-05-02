#pragma once
#include "Game/Entity.hpp"

class Game;
class Camera;

class Player : public Entity
{
public:
	Player(Game* owner);
	~Player();

	void Update(float deltaSeconds);
	void Render() const;

	Vec3 GetForwardVectorDueToOrientation() const;
	Vec3 GetLeftVectorDueToOrientation() const;

public:
	Vec3 m_originV;
	float m_originYaw;
	float m_originPitch;
	float m_originRoll;

	Camera m_worldCamera;
};