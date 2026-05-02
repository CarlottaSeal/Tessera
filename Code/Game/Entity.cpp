#include "Entity.hpp"
#include "Engine/Math/Mat44.hpp"

Entity::Entity(Game* owner)
	:m_game(owner)
{
}

Entity::~Entity()
{
}

Mat44 Entity::GetModelToWorldTransform() const
{
	Mat44 rotate;
	rotate = m_orientation.GetAsMatrix_IFwd_JLeft_KUp();
	Mat44 translate;
	translate = Mat44::MakeTranslation3D(m_position);

	translate.Append(rotate); //先旋转再平移
	return translate;
}
