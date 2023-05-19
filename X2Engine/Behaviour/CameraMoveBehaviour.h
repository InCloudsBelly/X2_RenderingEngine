#pragma once
#include "Core/Logic/Component/Base/Behaviour.h"
#include <string>
#include "Asset/Mesh.h"


class CameraMoveBehaviour : public Behaviour
{
public:
	CameraMoveBehaviour();
	~CameraMoveBehaviour();
	CameraMoveBehaviour(const CameraMoveBehaviour&) = delete;
	CameraMoveBehaviour& operator=(const CameraMoveBehaviour&) = delete;
	CameraMoveBehaviour(CameraMoveBehaviour&&) = delete;
	CameraMoveBehaviour& operator=(CameraMoveBehaviour&&) = delete;
	void onAwake()override;
	void onStart()override;
	void onUpdate()override;
	void onDestroy()override;

private:

	void processScroll4ScrollCallback(double vOffsetX, double vOffsetY);
	void processMouseButton4MouseButtonCallback(int vButton, int vAction, int vMods);


	double m_moveSpeed = 0.005;
	double m_sensitivity = 0.5;
	bool m_isEnableCursor = true;
private:

	RTTR_ENABLE(Behaviour)
};
