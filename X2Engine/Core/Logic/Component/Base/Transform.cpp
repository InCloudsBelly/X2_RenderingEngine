#include "Transform.h"
#include "Core/Logic/Object/GameObject.h"
#include <rttr/registration>
#include <glm/glm.hpp>

RTTR_REGISTRATION
{
    using namespace rttr;
    registration::class_<Transform>("Transform");
}

Transform::Transform()
    : Component(Component::ComponentType::TRANSFORM)
    , m_translation(glm::vec3(0, 0, 0))
    , m_rotation(glm::vec3(0, 0, 0))
    , m_scale(glm::vec3(1, 1, 1))
    , m_relativeModelMatrix(glm::mat4(1))
    , m_modelMatrix(glm::mat4(1))
{

}

Transform::~Transform()
{
}

bool Transform::active()
{
    return true;
}

void Transform::setActive(bool)
{
}

void Transform::updateModelMatrix(glm::mat4& parentModelMatrix)
{
    m_modelMatrix = parentModelMatrix * m_relativeModelMatrix;
    auto child = Child();
    while (child)
    {
        child->updateModelMatrix(m_modelMatrix);

        child = child->Brother();
    }
}

void Transform::setTranslation(glm::vec3 translation)
{
    this->m_translation = translation;

    m_relativeModelMatrix = getTranslationMatrix() * getRotationMatrix() * getScaleMatrix();
    updateModelMatrix(Parent()->m_modelMatrix);
}

void Transform::setRotation(glm::vec3 rotation)
{
    this->m_rotation = rotation;

    m_relativeModelMatrix = getTranslationMatrix() * getRotationMatrix() * getScaleMatrix();
    updateModelMatrix(Parent()->m_modelMatrix);
}

void Transform::setEulerRotation(glm::vec3 rotation)
{
    double k = std::acos(-1.0) / 180.0;
    this->m_rotation = rotation * static_cast<float>(k);

    m_relativeModelMatrix = getTranslationMatrix() * getRotationMatrix() * getScaleMatrix();
    updateModelMatrix(Parent()->m_modelMatrix);
}

void Transform::setScale(glm::vec3 scale)
{
    this->m_scale = scale;

    m_relativeModelMatrix = getTranslationMatrix() * getRotationMatrix() * getScaleMatrix();
    updateModelMatrix(Parent()->m_modelMatrix);
}

void Transform::setTranslationRotationScale(glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale)
{
    this->m_translation = translation;
    this->m_rotation = rotation;
    this->m_scale = scale;

    m_relativeModelMatrix = getTranslationMatrix() * getRotationMatrix() * getScaleMatrix();
    updateModelMatrix(Parent()->m_modelMatrix);
}

glm::mat4 Transform::getTranslationMatrix()
{
    return glm::mat4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        m_translation.x, m_translation.y, m_translation.z, 1
    );
}

glm::mat4 Transform::getRotationMatrix()
{
    return glm::rotate(glm::rotate(glm::rotate(glm::mat4(1), m_rotation.x, { 1, 0, 0 }), m_rotation.y, { 0, 1, 0 }), m_rotation.z, { 0, 0, 1 });
}

glm::mat4 Transform::getScaleMatrix()
{
    return glm::mat4(
        m_scale.x, 0, 0, 0,
        0, m_scale.y, 0, 0,
        0, 0, m_scale.z, 0,
        0, 0, 0, 1
    );
}

glm::mat4 Transform::getModelMatrix()
{
    return m_modelMatrix;
}

glm::vec3 Transform::getRotation()
{
    return m_rotation;
}

glm::vec3 Transform::getEulerRotation()
{
    const double k = 180.0 / std::acos(-1.0);
    return m_rotation * static_cast<float>(k);
}

glm::vec3 Transform::getTranslation()
{
    return m_translation;
}

glm::vec3 Transform::getScale()
{
    return m_scale;
}

void Transform::onAwake()
{
}

void Transform::onStart()
{
}

void Transform::onUpdate()
{
}

void Transform::onDestroy()
{
}
