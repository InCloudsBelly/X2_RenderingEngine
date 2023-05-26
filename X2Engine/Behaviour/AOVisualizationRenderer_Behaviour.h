#pragma once
#include "Core/Logic/Component/Base/Behaviour.h"
#include <string>
#include <future>

class Model;
class Shader;
class Image;


class AOVisualizationRenderer_Behaviour : public Behaviour
{
public:
	CONSTRUCTOR(AOVisualizationRenderer_Behaviour)

		AOVisualizationRenderer_Behaviour(std::string modelPath);
	void onAwake()override;
	void onStart()override;
	void onUpdate()override;
	void onDestroy()override;

private:
	Model* m_model;
	Shader* shader;

	RTTR_ENABLE(Behaviour)
};
