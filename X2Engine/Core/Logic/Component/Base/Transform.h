#pragma once
#include "Core/Logic/Component/Component.h"
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Utils/ChildBrotherTree.h"

class GameObject;
class Transform final : public Component, public ChildBrotherTree<Transform>
{
private:
	bool active();
	void setActive(bool);

	void updateModelMatrix(glm::mat4& parentModelMatrix);
	glm::vec3 m_rotation;
	glm::vec3 m_translation;
	glm::vec3 m_scale;

	glm::mat4 m_modelMatrix;
	glm::mat4 m_relativeModelMatrix;

public:
	void setTranslation(glm::vec3 translation);
	void setRotation(glm::vec3 rotation);
	void setEulerRotation(glm::vec3 rotation);
	void setScale(glm::vec3 scale);
	void setTranslationRotationScale(glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale);

	glm::mat4 getTranslationMatrix();
	glm::mat4 getRotationMatrix();
	glm::mat4 getScaleMatrix();
	glm::mat4 getModelMatrix();

	glm::vec3 getRotation();
	glm::vec3 getEulerRotation();
	glm::vec3 getTranslation();
	glm::vec3 getScale();

	void onAwake()override;
	void onStart()override;
	void onUpdate()override;
	void onDestroy()override;

	Transform();
	virtual ~Transform();

	RTTR_ENABLE(Component)

};