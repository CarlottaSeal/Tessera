#pragma once
#include "Game/Game.hpp"
#include "Engine/Math/EulerAngles.hpp"

struct Vec3;
struct Mat44;

class Entity
{
public:
	Entity(Game* owner);
	virtual ~Entity();

	virtual void Update(float deltaSeconds) = 0;
	virtual void Render() const = 0;
	virtual Mat44 GetModelToWorldTransform() const;

public:
	Game* m_game = nullptr;
	Vec3 m_position;
	Vec3 m_velocity;
	EulerAngles m_orientation;
	EulerAngles m_angularVelocity;
	Rgba8 m_color = Rgba8::GREY;
};