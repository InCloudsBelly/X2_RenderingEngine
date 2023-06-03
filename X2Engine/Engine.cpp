#include "Engine.h"

#include "Core/Logic/CoreObject/LogicInstance.h"

#include "Core/Logic/Manager/InputManager.h"

#include "Core/IO/Manager/AssetManager.h"

#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/Graphic/CoreObject/Window.h"
#include "Core/Graphic/CoreObject/Swapchain.h"

#include "Core/Graphic/Manager/DescriptorSetManager.h"
#include "Core/Graphic/Manager/RenderPassManager.h"
#include "Core/Graphic/Manager/RenderPipelineManager.h"
#include "Core/Graphic/Manager/LightManager.h"

#include "Core/Logic/Object/GameObject.h"
#include "Core/Logic/Component/Component.h"
#include "Core/Logic/Component/Base/Renderer.h"

#include "Core/Logic/Component/Base/CameraBase.h"
#include "Camera/PerspectiveCamera.h"

#include "Core/Graphic/Rendering/Shader.h"
#include "Core/Graphic/Rendering/RendererBase.h"
#include "Core/Graphic/Rendering/Material.h"
#include "Core/Graphic/Rendering/Framebuffer.h"



#include "Core/Graphic/Instance/Image.h"
#include "Core/Graphic/Instance/Buffer.h"

#include "Core/Graphic/Command/Semaphore.h"
#include "Core/Graphic/Command/Fence.h"
#include "Core/Graphic/Command/ImageMemoryBarrier.h"
#include "Core/Graphic/Command/CommandPool.h"
#include "Core/Graphic/Command/CommandBuffer.h"

#include "Rendering/RenderPipeline/EnumRenderPipeline.h"

#include "Rendering/Renderer/ForwardRenderer.h"

#include "Asset/Mesh.h"

#include "Light/DirectionalLight.h"
#include "Light/PointLight.h"
#include "Light/AmbientLight.h"

#include "Behaviour/CameraMoveBehaviour.h"
#include "Behaviour/BackgroundRenderer_Behaviour.h"
#include "Behaviour/PresentRenderer_Behaviour.h"
#include "Behaviour/SimpleForwardRenderer_Behaviour.h"
#include "Behaviour/AOVisualizationRenderer_Behaviour.h"

#include "Rendering/RenderFeature/PrefilteredEnvironmentMap_RenderFeature.h"
#include "Rendering/RenderFeature/PrefilteredIrradiance_RenderFeature.h"

#include "Utils/CrossLinkableNode.h"


#include <vma/vk_mem_alloc.h>



void Engine::run()
{
    init();
    prepareData();
    mainLoop();
    cleanup();
}


void Engine::init()
{
    LogicInstance::Init();
    Instance::Init();
}

void Engine::prepareData()
{
   Instance::getRenderPipelineManager()->switchRenderPipeline(new EnumRenderPipeline());


   Instance::getDescriptorSetManager().addDescriptorSetPool(ShaderSlotType::UNIFORM_BUFFER, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 10);
   Instance::getDescriptorSetManager().addDescriptorSetPool(ShaderSlotType::STORAGE_BUFFER, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER }, 10);
   Instance::getDescriptorSetManager().addDescriptorSetPool(ShaderSlotType::UNIFORM_TEXEL_BUFFER, { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER }, 10);
   Instance::getDescriptorSetManager().addDescriptorSetPool(ShaderSlotType::STORAGE_TEXEL_BUFFER, { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER }, 10);
   Instance::getDescriptorSetManager().addDescriptorSetPool(ShaderSlotType::TEXTURE_CUBE, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER }, 10);
   Instance::getDescriptorSetManager().addDescriptorSetPool(ShaderSlotType::TEXTURE2D, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER }, 10);
   Instance::getDescriptorSetManager().addDescriptorSetPool(ShaderSlotType::TEXTURE2D_WITH_INFO, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 10);
   Instance::getDescriptorSetManager().addDescriptorSetPool(ShaderSlotType::STORAGE_TEXTURE2D, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE }, 10);
   Instance::getDescriptorSetManager().addDescriptorSetPool(ShaderSlotType::STORAGE_TEXTURE2D_WITH_INFO, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 10);

   Instance::g_backgroundImage = Instance::getAssetManager()->load<Image>(std::string(SKYBOX_DIR) + "NeonStudio/neonStudio.hdr");


    //Camera
    GameObject* cameraGo = new GameObject("Camera");
    LogicInstance::rootObject.addChild(cameraGo);

    auto camera = new PerspectiveCamera(
        "ForwardRenderer",
        {
            {"ColorAttachment", Image::Create2DImage(Instance::g_swapchain->getExtent(), VK_FORMAT_R8G8B8A8_SRGB, VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT)},
            {"DepthAttachment", Image::Create2DImage(Instance::g_swapchain->getExtent(), VK_FORMAT_D32_SFLOAT, VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT)}
        }
    );
    CameraBase::mainCamera = camera;
    cameraGo->addComponent(camera);
    cameraGo->addComponent(new CameraMoveBehaviour());
    cameraGo->transform.setTranslation(glm::vec3(0,0, 0));



    //Renderers
    GameObject* renderers = new GameObject("Renderers");
    LogicInstance::rootObject.addChild(renderers);

    ///Background
    {
        GameObject* backgroundRendererGo = new GameObject("BackgroundRendererGameObject");
        renderers->addChild(backgroundRendererGo);
        backgroundRendererGo->addComponent(new Renderer());
        backgroundRendererGo->addComponent(new BackgroundRenderer_Behaviour());
    } 


    // Present
    {
        GameObject* presentRendererGo = new GameObject("PresentRendererGameObject");
        renderers->addChild(presentRendererGo);
        presentRendererGo->addComponent(new Renderer());
        presentRendererGo->addComponent(new PresentRenderer_Behaviour());

    }


    //Models
    GameObject* models = new GameObject("Models");
    LogicInstance::rootObject.addChild(models);


  /*  {
        GameObject* damaged_helmet = new GameObject("damaged_helmet_Model");
        models->addChild(damaged_helmet);
        damaged_helmet->addComponent(new SimpleForwardRenderer_Behaviour(std::string(MODEL_DIR) + "damagedHelmet/DamagedHelmet.gltf"));
        damaged_helmet->transform.setTranslation(glm::vec3(-10, 1, -3));
        damaged_helmet->transform.setScale(glm::vec3(5,5,5));
    }*/
    {
        GameObject* sponza = new GameObject("sponza");
        models->addChild(sponza);
        sponza->addComponent(new SimpleForwardRenderer_Behaviour(std::string(MODEL_DIR) + "sponzaTGA/SponzaPBR.obj"));
        sponza->transform.setTranslation(glm::vec3(0, 4, 10));
        sponza->transform.setScale(glm::vec3(1.5, 1.5, 1.5));
    }

    {
       GameObject* mrBalls = new GameObject("mrBalls");
       models->addChild(mrBalls);
       mrBalls->addComponent(new SimpleForwardRenderer_Behaviour(std::string(MODEL_DIR) + "MetalRoughSpheres/MetalRoughSpheres.gltf"));
       mrBalls->transform.setTranslation(glm::vec3(10, 10, -2.5));
       mrBalls->transform.setScale(glm::vec3(1.5, 1.5, 1.5));
   }
    
    {
        GameObject* plane = new GameObject("plane");
        models->addChild(plane);
        plane->addComponent(new SimpleForwardRenderer_Behaviour(std::string(MODEL_DIR) + "default/LargeQuad.ply"));
        plane->transform.setTranslation(glm::vec3(3, 0, -2.5));
        plane->transform.setScale(glm::vec3(100, 1, 100));
    }

    /*{
        GameObject* sponza = new GameObject("sponza");
        models->addChild(sponza);
        sponza->addComponent(new AOVisualizationRenderer_Behaviour(std::string(MODEL_DIR) + "sponzaTGA/SponzaPBR.obj"));
        sponza->transform.setTranslation(glm::vec3(0, 4, 10));
        sponza->transform.setScale(glm::vec3(1.5, 1.5, 1.5));
    }

    {
        GameObject* damaged_helmet = new GameObject("damaged_helmet_Model");
        models->addChild(damaged_helmet);
        damaged_helmet->addComponent(new AOVisualizationRenderer_Behaviour(std::string(MODEL_DIR) + "damagedHelmet/DamagedHelmet.gltf"));
        damaged_helmet->transform.setTranslation(glm::vec3(-10, 1, -3));
        damaged_helmet->transform.setScale(glm::vec3(5, 5, 5));
    }

    {
        GameObject* plane = new GameObject("plane");
        models->addChild(plane);
        plane->addComponent(new AOVisualizationRenderer_Behaviour(std::string(MODEL_DIR) + "default/LargeQuad.ply"));
        plane->transform.setTranslation(glm::vec3(3, 0, -2.5));
        plane->transform.setScale(glm::vec3(100, 1, 100));
    }*/




    //Lights
    GameObject* lights = new GameObject("Lights");
    LogicInstance::rootObject.addChild(lights);

    GameObject* directionalLightGo = new GameObject("DirectionalLight");
    lights->addChild(directionalLightGo);
    directionalLightGo->transform.setEulerRotation(glm::vec3(-60, 0, 0));
    auto directionalLight = new DirectionalLight();
    directionalLight->color = { 1, 239.0 / 255, 213.0 / 255, 1 };
    directionalLight->intensity = 8;
    directionalLightGo->addComponent(directionalLight);
   
    GameObject* iblGo = new GameObject("SkyBox");
    lights->addChild(iblGo);
    auto iblLight = new AmbientLight();
    iblLight->color = { 1, 1, 1, 1 };
    iblLight->intensity = 0.5f;
    iblLight->m_lutImage = Instance::getAssetManager()->load<Image>(std::string(MODEL_DIR) + "defaultTexture/BRDF_LUT.png");
    iblLight->m_irradianceCubeImage = static_cast<PrefilteredIrradiance_RenderFeature::PrefilteredIrradiance_RenderFeatureData*>(camera->getRendererData()->getRenderFeatureData("PrefilteredIrradiance_RenderFeature"))->m_targetCubeImage;
    iblLight->m_prefilteredCubeImage = static_cast<PrefilteredEnvironmentMap_RenderFeature::PrefilteredEnvironmentMap_RenderFeatureData*>(camera->getRendererData()->getRenderFeatureData("PrefilteredEnvironmentMap_RenderFeature"))->m_targetCubeImage;

    iblGo->addComponent(iblLight);



}

void Engine::mainLoop()
{
    iterateByDynamicBfs(Component::ComponentType::BEHAVIOUR);

    auto targetComponents = std::vector<std::vector<Component*>>();
    iterateByStaticBfs({ Component::ComponentType::LIGHT, Component::ComponentType::CAMERA, Component::ComponentType::RENDERER }, targetComponents);

    Instance::addLight(targetComponents[0]);
    Instance::addCamera(targetComponents[1]);
    Instance::addRenderer(targetComponents[2]);

    Instance::getLightManager().setLightInfo(Instance::g_lights);

    uint8_t currentFrame = 0;


    while (Instance::getWindow()->isWindowClosed() == false)
    {
        LogicInstance::m_time.refresh();
        Instance::getWindow()->pollEvents();

        iterateByDynamicBfs(Component::ComponentType::BEHAVIOUR);
        LogicInstance::rootObject.m_gameObject.transform.updateModelMatrix(glm::mat4(1.0));

        std::vector<Renderer*> rendererComponents = std::vector<Renderer*>(Instance::g_renderers.size());
        for (size_t i = 0; i < rendererComponents.size(); i++)
        {
            rendererComponents[i] = static_cast<Renderer*>(Instance::g_renderers[i]);
            rendererComponents[i]->refreshObjectInfo();
        }

        //Camera
        for (auto& component : Instance::g_cameras)
        {
            auto camera = dynamic_cast<CameraBase*>(component);
            if (!(camera)) continue;
            camera->refreshCameraInfo();

            auto renderer = Instance::getRenderPipelineManager()->getRenderer(camera->getRendererName());
            auto rendererData = Instance::getRenderPipelineManager()->getRendererData(camera);
            renderer->prepareRenderer(rendererData);
        }
        for (auto& component : Instance::g_cameras)
        {
            auto camera = dynamic_cast<CameraBase*>(component);
            if (!(camera)) continue;
            auto renderer = Instance::getRenderPipelineManager()->getRenderer(camera->getRendererName());
            auto rendererData = Instance::getRenderPipelineManager()->getRendererData(camera);
            renderer->excuteRenderer(rendererData, camera, &rendererComponents);
        }
        for (auto& component : Instance::g_cameras)
        {
            auto camera = dynamic_cast<CameraBase*>(component);
            if (!(camera)) continue;
            auto renderer = Instance::getRenderPipelineManager()->getRenderer(camera->getRendererName());
            auto rendererData = Instance::getRenderPipelineManager()->getRendererData(camera);
            renderer->submitRenderer(rendererData);
        }
        for (auto& component : Instance::g_cameras)
        {
            auto camera = dynamic_cast<CameraBase*>(component);
            if (!(camera)) continue;
            auto renderer = Instance::getRenderPipelineManager()->getRenderer(camera->getRendererName());
            auto rendererData = Instance::getRenderPipelineManager()->getRendererData(camera);
            renderer->finishRenderer(rendererData);
        }



        Instance::getDescriptorSetManager().collect();
        Instance::getRenderPassManager().collect();
        Instance::getAssetManager()->collect();

        //currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    vkDeviceWaitIdle(Instance::getDevice());

    Instance::clearLight();
    Instance::clearCamera();
    Instance::clearRenderer();


    Instance::getDescriptorSetManager().collect();
    Instance::getRenderPassManager().collect();
    Instance::getAssetManager()->collect();
}


void Engine::cleanup()
{

    //for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    //{
    //    delete  m_imageAvailableSemaphores[i];
    //    delete  m_renderFinishedSemaphores[i];
    //}
    Instance::getAssetManager()->unload(Instance::getLightManager().getAmbientLight()->m_lutImage);

    destroyByStaticBfs( { Component::ComponentType::BEHAVIOUR, Component::ComponentType::CAMERA, Component::ComponentType::RENDERER });

    Instance::destroy();
    LogicInstance::destroy();
}

void Engine::destroyByStaticBfs(std::vector<Component::ComponentType> targetComponentTypes)
{
    std::list< GameObject*> curGenGameObjectHeads = std::list<GameObject*>();
    std::list< GameObject*> nextGenGameObjectHeads = std::list<GameObject*>();

    //Init
    if (LogicInstance::rootObject.m_gameObject.getChild())
    {
        curGenGameObjectHeads.emplace_back(LogicInstance::rootObject.m_gameObject.getChild());
    }

    while (!curGenGameObjectHeads.empty())
    {
        for (const auto& curGenGameObjectHead : curGenGameObjectHeads)
        {
            GameObject* curGenGameObject = curGenGameObjectHead;
            while (curGenGameObject)
            {
                //Update components
                for (uint32_t i = 0; i < targetComponentTypes.size(); i++)
                {
                    if (curGenGameObject->m_typeSqueueComponentsHeadMap.count(targetComponentTypes[i]))
                    {
                        for (auto iterator = curGenGameObject->m_typeSqueueComponentsHeadMap[targetComponentTypes[i]]->GetIterator(); iterator.IsValid(); iterator++)
                        {
                            auto component = static_cast<Component*>(iterator.Node());
                            component->onDestroy();          
                        }
                    }
                }

                if (curGenGameObject->getChild())
                {
                    auto childHead = curGenGameObject->getChild();
                    nextGenGameObjectHeads.emplace_back(childHead);
                }
                curGenGameObject = curGenGameObject->getBrother();
            }
        }
        curGenGameObjectHeads.clear();
        std::swap(nextGenGameObjectHeads, curGenGameObjectHeads);
    }
}

void Engine::iterateByStaticBfs(std::vector<Component::ComponentType> targetComponentTypes, std::vector<std::vector<Component*>>& targetComponents)
{
    std::string targetComponentTypeString = "";
    for (const auto& type : targetComponentTypes)
    {
        targetComponentTypeString += std::to_string(static_cast<int>(type)) + " ";
    }

    std::list< GameObject*> curGenGameObjectHeads = std::list<GameObject*>();
    std::list< GameObject*> nextGenGameObjectHeads = std::list<GameObject*>();
    targetComponents.clear();
    targetComponents.resize(targetComponentTypes.size());

    //Init
    if (LogicInstance::rootObject.m_gameObject.getChild())
    {
        curGenGameObjectHeads.emplace_back(LogicInstance::rootObject.m_gameObject.getChild());
    }

    while (!curGenGameObjectHeads.empty())
    {
        for (const auto& curGenGameObjectHead : curGenGameObjectHeads)
        {
            GameObject* curGenGameObject = curGenGameObjectHead;
            while (curGenGameObject)
            {
                //Update components
                for (uint32_t i = 0; i < targetComponentTypes.size(); i++)
                {
                    if (curGenGameObject->m_typeSqueueComponentsHeadMap.count(targetComponentTypes[i]))
                    {
                        for (auto iterator = curGenGameObject->m_typeSqueueComponentsHeadMap[targetComponentTypes[i]]->GetIterator(); iterator.IsValid(); iterator++)
                        {
                            auto component = static_cast<Component*>(iterator.Node());

                            component->update();

                            targetComponents[i].emplace_back(component);
                        }
                    }
                }

                //Add next gen GameObject
                if (curGenGameObject->getChild())
                {
                    auto childHead = curGenGameObject->getChild();

                    nextGenGameObjectHeads.emplace_back(childHead);
                }

                curGenGameObject = curGenGameObject->getBrother();
            }

        }

        curGenGameObjectHeads.clear();
        std::swap(nextGenGameObjectHeads, curGenGameObjectHeads);
    }
}

void Engine::iterateByDynamicBfs(Component::ComponentType targetComponentType)
{
    std::list< GameObject*> curGenGameObjectHeads = std::list<GameObject*>();
    std::list< GameObject*> nextGenGameObjectHeads = std::list<GameObject*>();
    auto& validGameObjectInIteration = LogicInstance::g_validGameObjectInIteration;
    auto& validComponentInIteration = LogicInstance::g_validComponentInIteration;

    //Clear
    validGameObjectInIteration.clear();
    validComponentInIteration.clear();
    curGenGameObjectHeads.clear();
    nextGenGameObjectHeads.clear();

    //Init
    if (LogicInstance::rootObject.m_gameObject.getChild())
    {
        validGameObjectInIteration.emplace(LogicInstance::rootObject.m_gameObject.getChild());

        curGenGameObjectHeads.emplace_back(LogicInstance::rootObject.m_gameObject.getChild());
    }

    while (!curGenGameObjectHeads.empty())
    {
        for (const auto& curGenGameObjectHead : curGenGameObjectHeads)
        {
            if (!validGameObjectInIteration.count(curGenGameObjectHead)) continue;

            std::vector<GameObject*> curGenGameObjects = std::vector<GameObject*>();
            {
                curGenGameObjects.emplace_back(curGenGameObjectHead);
                GameObject* gameObject = curGenGameObjectHead->getBrother();
                while (gameObject)
                {
                    curGenGameObjects.emplace_back(gameObject);
                    validGameObjectInIteration.emplace(gameObject);

                    gameObject = gameObject->getBrother();
                }
            }

            for (const auto& curGenGameObject : curGenGameObjects)
            {
                if (!validGameObjectInIteration.count(curGenGameObject))continue;

                //Run Components
                validComponentInIteration.clear();
                if (curGenGameObject->m_typeSqueueComponentsHeadMap.count(targetComponentType))
                {
                    std::vector< Component*> components = std::vector< Component*>();
                    for (auto iterator = curGenGameObject->m_typeSqueueComponentsHeadMap[targetComponentType]->GetIterator(); iterator.IsValid(); iterator++)
                    {
                        auto component = static_cast<Component*>(iterator.Node());

                        validComponentInIteration.insert(component);
                        components.emplace_back(component);
                    }

                    for (const auto& component : components)
                    {
                        if (validComponentInIteration.count(component)) component->update();
                    }
                }

                if (!validGameObjectInIteration.count(curGenGameObject))continue;
                if (curGenGameObject->getChild())
                {
                    auto childHead = curGenGameObject->getChild();

                    nextGenGameObjectHeads.emplace_back(childHead);
                    validGameObjectInIteration.emplace(childHead);
                }
            }
        }

        curGenGameObjectHeads.clear();
        std::swap(nextGenGameObjectHeads, curGenGameObjectHeads);
    }

    validGameObjectInIteration.clear();
    validComponentInIteration.clear();
}
