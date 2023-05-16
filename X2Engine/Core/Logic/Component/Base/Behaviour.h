#pragma once
#include "Core/Logic/Component/Component.h"


class Behaviour : public Component
{
public:
	Behaviour();
	virtual ~Behaviour();
	virtual void onAwake()override;
	virtual void onStart()override;
	virtual void onUpdate()override;
	virtual void onDestroy()override;
	Behaviour(const Behaviour&) = delete;
	Behaviour& operator=(const Behaviour&) = delete;
	Behaviour(Behaviour&&) = delete;
	Behaviour& operator=(Behaviour&&) = delete;
};