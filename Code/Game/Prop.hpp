#pragma once
#include "Game/Entity.hpp"
#include <vector>

class Game;
class Texture;

enum PropType
{
	Cube,
	Sphere,
	Count
};

class Prop : public Entity
{
public:
	Prop(Game* owner, PropType propType);
	~Prop();

	virtual void Update(float deltaSeconds) override;
	void UpdateColor(float deltaSeconds);
	void UpdateRotate(float deltaSeconds);
	void UpdateRotateForSphere(float deltaSeconds);
	virtual void Render() const override;
	void InitializeVertsAndTexture();

public:
	PropType m_type = PropType::Cube;

	std::vector<Vertex_PCU> m_vertexes;
	Rgba8 m_color = Rgba8::WHITE;
	Texture* m_texture = nullptr;
};