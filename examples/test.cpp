#include "vksim/core/resources/Material.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "vksim/core/buffers/Image.hpp"
#include "vksim/core/context/CommandPool.hpp"
#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/render/Renderer.hpp"
#include "vksim/core/render/Swapchain.hpp"
#include "vksim/core/resources/Material.hpp"
#include "vksim/core/resources/Mesh.hpp"
#include "vksim/core/resources/ResourceManager.hpp"
#include "vksim/core/resources/Texture.hpp"
#include "vksim/core/scene/Camera.hpp"
#include "vksim/core/scene/Scene.hpp"
#include "vksim/core/scene/SceneObject.hpp"
#include "vksim/core/window/Window.hpp"
#include "vksim/slang/SlangCompiler.hpp"
#include "vksim/utility/Error.hpp"
#include "vksim/utility/Logging.hpp"

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

#ifdef DEBUG
constexpr bool enableValidationLayers = true;
#else
constexpr bool enableValidationLayers = false;
#endif

auto main() -> int
{

  // Set the runtime log level to info
  vksim::logging::set_runtime_log_level();

  // Create a window with the specified width and height
  auto window = vksim::Window(WIDTH, HEIGHT);

  // Create a Vulkan context with the specified instance and device creation info
  auto context = vksim::VulkanContext(
      window, {.instance = {.appName = "Hello Triangle",
                            .appVersion = VK_MAKE_VERSION(1, 0, 0),
                            .engineName = "No Engine",
                            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                            .apiVersion = VK_API_VERSION_1_3,
                            .layers = {"VK_LAYER_KHRONOS_validation"},
                            .extensions = {vk::KHRPortabilityEnumerationExtensionName}},

               .device = {
                   .extensions = {vk::KHRSwapchainExtensionName, "VK_KHR_portability_subset"},
                   .features =
                       {
                           .anisotropicFiltering = true,
                           .shaderDrawParameters = true,
                           .dynamicRendering = true,
                           .synchronization2 = true,
                           .extendedDynamicState = true,
                           .runtimeDescriptorArray = true,
                       },
               }});

  // Request a graphics queue and a compute queue from the Vulkan context
  auto &graphicsQueue = context.requestQueue(
      {.requiredFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eTransfer,
       .requiresPresent = true});

  // Request a compute queue from the Vulkan context
  auto &computeQueue = context.requestQueue(
      {.requiredFlags = vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer,
       .requiresPresent = false});

  // Build the Vulkan context, which creates the Vulkan instance, selects a physical device, creates
  // a logical device, and sets up the swap chain and command pool and allocates the requested
  // queues.
  context.build();

  // Create a resource manager to manage meshes and textures
  vksim::ResourceManager resourceManager;

  // Create an upload context to handle resource uploads to the GPU
  vksim::UploadContext uploadContext(context);
  uploadContext.begin();

  // Load a mesh and a texture using the resource manager
  resourceManager.Load<vksim::Mesh>("floor_mesh", context, uploadContext,
                                    PROJECT_SOURCE_DIR "/assets/meshes/floor.obj");

  resourceManager.Load<vksim::Texture>("floor_texture", context, uploadContext,
                                       PROJECT_SOURCE_DIR "/assets/textures/floor.png");

  resourceManager.Load<vksim::Mesh>("ball_mesh", context, uploadContext,
                                    PROJECT_SOURCE_DIR "/assets/meshes/ball.obj");

  resourceManager.Load<vksim::Mesh>("teapot_mesh", context, uploadContext,
                                    PROJECT_SOURCE_DIR "/assets/meshes/utah_teapot.obj");

  resourceManager.Load<vksim::Mesh>("bunny_mesh", context, uploadContext,
                                    PROJECT_SOURCE_DIR "/assets/meshes/bunny.obj");
  // Load material one
  {
    vksim::MaterialInfo materialInfo{
        .m_baseColor = glm::vec3(0.0F, 0.0F, 1.0F), .m_metallic = 0.0F, .m_roughness = 0.1F};

    resourceManager.Load<vksim::Material>("material_1", context, uploadContext, materialInfo);
  }
  // Load material two
  {
    vksim::MaterialInfo materialInfo{
        .m_baseColor = glm::vec3(0.0F, 1.0F, 0.0F), .m_metallic = 0.0F, .m_roughness = 0.4F};

    resourceManager.Load<vksim::Material>("material_2", context, uploadContext, materialInfo);
  }
  // Load material three
  {
    vksim::MaterialInfo materialInfo{
        .m_baseColor = glm::vec3(1.0F, 0.0F, 0.0F), .m_metallic = 0.7F, .m_roughness = 0.1F};

    resourceManager.Load<vksim::Material>("material_3", context, uploadContext, materialInfo);
  }

  // Submit the upload context and wait for completion
  uploadContext.submitAndWait();

  // Create a scene and add a camera and an object with the loaded mesh and texture
  auto scene = vksim::Scene(context, resourceManager);
  auto &camera = scene.addCamera();

  auto &spotLight = scene.addSpotLight();
  auto &pointLight = scene.addPointLight();

  auto &ball = scene.addObject();
  ball.setMesh("ball_mesh");
  ball.setMaterial("material_1");
  ball.setVisible(true);

  auto &bunny = scene.addObject();
  bunny.setMesh("bunny_mesh");
  bunny.setMaterial("material_2");
  bunny.setVisible(true);

  auto &teapot = scene.addObject();
  teapot.setMesh("teapot_mesh");
  teapot.setMaterial("material_3");
  teapot.setVisible(true);

  auto &floor = scene.addObject();
  floor.setMesh("floor_mesh");
  floor.setTexture("floor_texture");
  floor.setVisible(true);

  floor.transform({.position = glm::vec3(0.F, 0.F, 0.F),
                   .rotation = glm::vec3(0.0F, 0.0F, 0.0F),
                   .scale = glm::vec3(10.0F, 10.0F, 10.0F)});

  bunny.transform({.position = glm::vec3(0.F, -4.0F, 0.0F),
                   .rotation = glm::vec3(90.0F, 0.0F, 0.0F),
                   .scale = glm::vec3(15.0F, 15.0F, 15.0F)});

  ball.transform({.position = glm::vec3(0.F, 4.F, 1.0F),
                  .rotation = glm::vec3(0.0F, 0.0F, 0.0F),
                  .scale = glm::vec3(1.0F, 1.0F, 1.0F)});

  camera.transform({.width = WIDTH,
                    .height = HEIGHT,
                    .position = glm::vec3(5.0F, 5.0F, 5.5F),
                    .center = glm::vec3(0.0F, 0.0F, 0.0F),
                    .up = glm::vec3(0.0F, 0.0F, 1.0F),
                    .fov = 90.0F,
                    .nearPlane = 0.1F,
                    .farPlane = 100.0F});

  spotLight.transform({.position = glm::vec3(0.0F, 0.0F, 5.0F),
                       .direction = glm::vec3(0.0F, 0.0F, -1.0F),
                       .color = glm::vec3(1.0F, 1.0F, 1.0F),
                       .innerCone = glm::radians(45.0F),
                       .outerCone = glm::radians(80.0F),
                       .intensity = 10.0F});

  pointLight.transform({.position = glm::vec3(5.0F, 5.0F, 5.0F),
                        .color = glm::vec3(1.0F, 1.0F, 1.0F),
                        .intensity = 10.0F});

  // Create a renderer with the Vulkan context and the scene
  auto renderer = vksim::Renderer(context, scene, graphicsQueue, MAX_FRAMES_IN_FLIGHT);

  while (!window.shouldClose())
  {
    vksim::Window::pollEvents();

    // curcular camera movement around the scene
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time =
        std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    camera.transform(
        {.position = glm::vec3(sin(time * 0.5F) * 5.0F, cos(time * 0.5F) * 5.0F, 5.5F)});
    pointLight.transform(
        {.position = glm::vec3(sin(time * 0.5F) * 5.0F, cos(time * 0.5F) * 5.0F, 5.0F)});

    renderer.drawFrame();
  }
  context.getDevice().logical().waitIdle();
}
