#pragma once
#include "Core/Logic/Object/GameObject.h"
#include <unordered_set>
#include "Utils/Time.h"

class GameObject;
class Component;

class InputManager;


//class Thread;
class LogicInstance final
{
	friend class Engine;
	friend class GameObject;
public:
	class RootGameObject final
	{
		friend class LogicInstance;
		friend class Engine;
	private:
		GameObject m_gameObject;
		RootGameObject();
		~RootGameObject();
	public:
		inline void addChild(GameObject* child);
		inline void removeChild(GameObject* child);
		inline GameObject* getChild();
	};

	static RootGameObject rootObject;
	static Time m_time;
	//static void exit();
	//static void waitExit();
	static bool needIterateRenderer();
	static void setNeedIterateRenderer(bool needIterateRenderer);
	static InputManager* getInputManager();

	static void Init();
	static void destroy();

private:
	//static Utils::Condition* _exitCondition;
	static InputManager* g_inputManager;
	static bool g_needIterateRenderer;
	static std::unordered_set< GameObject*> g_validGameObjectInIteration;
	static std::unordered_set< Component*> g_validComponentInIteration;
	LogicInstance();
	~LogicInstance();
};


inline void LogicInstance::RootGameObject::addChild(GameObject* child)
{
	m_gameObject.addChild(child);
}

inline void LogicInstance::RootGameObject::removeChild(GameObject* child)
{
	m_gameObject.removeChild(child);
}

inline GameObject* LogicInstance::RootGameObject::getChild()
{
	return m_gameObject.getChild();
}