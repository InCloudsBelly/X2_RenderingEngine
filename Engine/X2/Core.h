//
// Note:	this file is to be included in client applications ONLY
//			NEVER include this file anywhere in the engine codebase
//
#pragma once

#include "X2/Core/Application.h"
#include "X2/Core/Log.h"
#include "X2/Core/Input.h"
#include "X2/Core/TimeStep.h"
#include "X2/Core/Timer.h"

#include "X2/Core/Event/Event.h"
#include "X2/Core/Event/ApplicationEvent.h"
#include "X2/Core/Event/KeyEvent.h"
#include "X2/Core/Event/MouseEvent.h"
#include "X2/Core/Event/SceneEvent.h"

#include "X2/Math/AABB.h"
#include "X2/Math/Ray.h"

#include "imgui/imgui.h"

// --- Render API ------------------------------
#include "X2/Renderer/Renderer.h"
#include "X2/Renderer/SceneRenderer.h"
#include "X2/Renderer/Mesh.h"
#include "X2/Renderer/Camera.h"

#include "X2/Vulkan/VulkanRenderPass.h"
#include "X2/Vulkan/VulkanFramebuffer.h"
#include "X2/Vulkan/VulkanVertexBuffer.h"
#include "X2/Vulkan/VulkanIndexBuffer.h"
#include "X2/Vulkan/VulkanPipeline.h"
#include "X2/Vulkan/VulkanTexture.h"
#include "X2/Vulkan/VulkanShader.h"

#include "X2/Vulkan/VulkanMaterial.h"
// ---------------------------------------------------

// Scenes
#include "X2/Scene/Entity.h"
#include "X2/Scene/Scene.h"
#include "X2/Scene/SceneCamera.h"
#include "X2/Scene/SceneSerializer.h"
#include "X2/Scene/Components.h"
