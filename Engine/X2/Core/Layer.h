#pragma once

#include "X2/Core/Event/Event.h"
#include "X2/Core/Timestep.h"

#include <string>

namespace X2 {

	class Layer
	{
	public:
		Layer(const std::string& name = "Layer") : m_DebugName(name) {};
		virtual ~Layer() {};

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate(Timestep ts) {}
		virtual void OnImGuiRender() {}
		virtual void OnEvent(Event& event) {}

		inline const std::string& GetName() const { return m_DebugName; }
	protected:
		std::string m_DebugName;
	};

}
