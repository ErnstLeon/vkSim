#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/render/Renderer.hpp"
#include "vksim/core/render/Swapchain.hpp"
#include "vksim/imgui/ImGuiUtil.hpp"
#include "vksim/slang/SlangCompiler.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim
{
Renderer::Renderer(VulkanContext &context, Scene &scene, QueueHandle &queueHandle,
                   uint32_t framesInFlight)
    : m_context(context), m_scene(scene), m_queueHandle(queueHandle),
      m_framesInFlight(framesInFlight), m_swapchain(context)
{
  createSceneResources();
  createCameraResources();
  createLightResources();
  extractUniqueMaterialsAndTextures();
  createSwapchain();
  createDepthResources();
  createColorResources();
  createDescriptorPool();
  createDescriptorSetLayouts();
  createDescriptorSets();
  createGraphicsPipeline();
  createCommandBuffers();
  createSyncObjects();
  prepareImGui();
}

auto Renderer::prepareImGui() -> void
{
  m_imguiRenderer = std::make_unique<vksim::ImGui::ImGuiRenderer>(m_context, m_swapchain, m_scene);
  m_imguiRenderer->init();
}

auto Renderer::createSceneResources() -> void
{
  // Create uniform buffers for the scene information for each frame in flight. The size of the
  // uniform buffer is the size of the SceneInfo struct. The uniform buffer is HostVisible and
  // HostCoherent to allow for CPU-side updates.
  m_SceneInfoUniformBuffer.reserve(m_framesInFlight);
  m_SceneInfoUniformBufferMapped.reserve(m_framesInFlight);

  for (uint32_t i = 0; i < m_framesInFlight; ++i)
  {
    m_SceneInfoUniformBuffer.emplace_back(m_context.getDevice());
    m_SceneInfoUniformBuffer.back().create(
        BufferCreateInfo{.size = sizeof(Scene::SceneInfo),
                         .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                         .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                       vk::MemoryPropertyFlagBits::eHostCoherent});
    m_SceneInfoUniformBufferMapped.emplace_back(
        m_SceneInfoUniformBuffer.back().getVkBufferMemory().mapMemory(0, sizeof(Scene::SceneInfo)));
  }
}

auto Renderer::createCameraResources() -> void
{
  // Create uniform buffers for the camera for each frame in flight as the camera's uniform buffer
  // will be updated each frame.
  m_cameraUniformBuffers.reserve(m_framesInFlight);
  m_cameraUniformBuffersMapped.reserve(m_framesInFlight);

  for (uint32_t i = 0; i < m_framesInFlight; ++i)
  {
    m_cameraUniformBuffers.emplace_back(m_context.getDevice());
    m_cameraUniformBuffers.back().create(
        BufferCreateInfo{.size = sizeof(Camera::Params),
                         .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                         .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                       vk::MemoryPropertyFlagBits::eHostCoherent});
    m_cameraUniformBuffersMapped.emplace_back(
        m_cameraUniformBuffers.back().getVkBufferMemory().mapMemory(0, sizeof(Camera::Params)));
  }
}

auto Renderer::createLightResources() -> void
{
  auto numDirectionalLights = static_cast<uint32_t>(m_scene.getDirectionalLights().size());
  auto numPointLights = static_cast<uint32_t>(m_scene.getPointLights().size());
  auto numSpotLights = static_cast<uint32_t>(m_scene.getSpotLights().size());
  const uint32_t directionalLightBufferCount = std::max(1U, numDirectionalLights);
  const uint32_t pointLightBufferCount = std::max(1U, numPointLights);
  const uint32_t spotLightBufferCount = std::max(1U, numSpotLights);

  m_directionalLightBuffers.reserve(m_framesInFlight);
  m_directionalLightBuffersMapped.reserve(m_framesInFlight);

  // Create storage buffers for directional lights for each frame in flight. Allocate at least one
  // element so descriptor bindings remain valid even when the scene has no directional lights.
  for (uint32_t i = 0; i < m_framesInFlight; ++i)
  {
    const vk::DeviceSize bufferSize =
        sizeof(DirectionalLight::Params) * directionalLightBufferCount;
    m_directionalLightBuffers.emplace_back(m_context.getDevice());
    m_directionalLightBuffers.back().create(
        BufferCreateInfo{.size = bufferSize,
                         .usage = vk::BufferUsageFlagBits::eStorageBuffer,
                         .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                       vk::MemoryPropertyFlagBits::eHostCoherent});
    void *mappedMemory =
        m_directionalLightBuffers.back().getVkBufferMemory().mapMemory(0, bufferSize);
    std::memset(mappedMemory, 0, static_cast<size_t>(bufferSize));
    m_directionalLightBuffersMapped.emplace_back(mappedMemory);
  }

  m_pointLightBuffers.reserve(m_framesInFlight);
  m_pointLightBuffersMapped.reserve(m_framesInFlight);

  // Create storage buffers for point lights for each frame in flight. Allocate at least one
  // element so descriptor bindings remain valid even when the scene has no point lights.
  for (uint32_t i = 0; i < m_framesInFlight; ++i)
  {
    const vk::DeviceSize bufferSize = sizeof(PointLight::Params) * pointLightBufferCount;
    m_pointLightBuffers.emplace_back(m_context.getDevice());
    m_pointLightBuffers.back().create(
        BufferCreateInfo{.size = bufferSize,
                         .usage = vk::BufferUsageFlagBits::eStorageBuffer,
                         .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                       vk::MemoryPropertyFlagBits::eHostCoherent});
    void *mappedMemory = m_pointLightBuffers.back().getVkBufferMemory().mapMemory(0, bufferSize);
    std::memset(mappedMemory, 0, static_cast<size_t>(bufferSize));
    m_pointLightBuffersMapped.emplace_back(mappedMemory);
  }

  m_spotLightBuffers.reserve(m_framesInFlight);
  m_spotLightBuffersMapped.reserve(m_framesInFlight);

  // Create storage buffers for spot lights for each frame in flight. Allocate at least one
  // element so descriptor bindings remain valid even when the scene has no spot lights.
  for (uint32_t i = 0; i < m_framesInFlight; ++i)
  {
    const vk::DeviceSize bufferSize = sizeof(SpotLight::Params) * spotLightBufferCount;
    m_spotLightBuffers.emplace_back(m_context.getDevice());
    m_spotLightBuffers.back().create(
        BufferCreateInfo{.size = bufferSize,
                         .usage = vk::BufferUsageFlagBits::eStorageBuffer,
                         .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                       vk::MemoryPropertyFlagBits::eHostCoherent});
    void *mappedMemory = m_spotLightBuffers.back().getVkBufferMemory().mapMemory(0, bufferSize);
    std::memset(mappedMemory, 0, static_cast<size_t>(bufferSize));
    m_spotLightBuffersMapped.emplace_back(mappedMemory);
  }

  if (numDirectionalLights == 0 && numPointLights == 0 && numSpotLights == 0)
  {
    spdlog::warn("No lights found in the scene. No light uniform buffers will be created.");
  }
}

auto Renderer::extractUniqueMaterialsAndTextures() -> void
{
  m_objectDescriptors.clear();
  m_uniqueMaterials.clear();
  m_uniqueTextures.clear();
  m_objectDescriptors.reserve(m_scene.getObjects().size());

  // temporary maps to track the unique materials and textures and their corresponding indices in
  // the uniqueMaterials and uniqueTextures vectors.
  std::unordered_map<std::string, uint32_t> materialIdMap;
  std::unordered_map<std::string, uint32_t> textureIdMap;

  auto getOrAddResourceIndex = [&materialIdMap, &textureIdMap](auto &uniqueResources,
                                                               const auto &resource) -> uint32_t
  {
    if (!resource)
    {
      return static_cast<uint32_t>(~0);
    }

    // Check if the resource is already in the uniqueResources vector. If not, add it and return
    // its index.
    auto [iterator, inserted] = materialIdMap.try_emplace(
        resource.value()->GetId(), static_cast<uint32_t>(uniqueResources.size()));
    if (inserted)
    {
      uniqueResources.push_back(resource.value()->GetId());
    }

    return iterator->second;
  };

  // Loop over all object in the scene and count the number of unique materials and textures that
  // are used by the objects. This is needed to create the descriptor pool with the correct number
  // of descriptors for each type. Loop over all objects in the scene and cache descriptor data in
  // scene order. Visibility is a runtime UI state, so descriptor indices must stay stable even when
  // objects are hidden.
  for (const auto &object : m_scene.getObjects())
  {
    const auto material = object->getMaterial();
    const auto texture = object->getTexture();

    if (!material && !texture)
    {
      spdlog::warn("SceneObject {} has no material or texture assigned", object->getObjectId());
    }

    // Use an invalid sentinel when no resource is assigned.
    const uint32_t materialId = getOrAddResourceIndex(m_uniqueMaterials, material);
    const uint32_t textureId = getOrAddResourceIndex(m_uniqueTextures, texture);

    m_objectDescriptors.push_back(
        {.modelMatrix = object->getModelMatrix(),
         // transpose first, as the model matrix was transposed for column-major order in the
         // vertex shader, and then invert to get the normal matrix. The normal matrix is supposed
         // to be transposed again. As Vulkan uses column-major order, we can skip the second
         // transpose as it is done implicitly by the column-major order.
         .normalMatrix =
             glm::mat4(glm::inverse(glm::transpose(glm::mat3(object->getModelMatrix())))),
         .textureId = textureId,
         .materialId = materialId});
  }
}

auto Renderer::createSwapchain() -> void
{
  m_swapchain.create(SwapchainCreateInfo{.format = vk::Format::eB8G8R8A8Srgb,
                                         .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
                                         .imageCount = 3});
}

auto Renderer::createDepthResources() -> void
{
  m_depthImage.emplace(m_context.getDevice());
  m_depthImage->create({.width = m_swapchain.getExtent().width,
                        .height = m_swapchain.getExtent().height,
                        .numSamples = m_context.getMaxUsableSampleCount(),
                        .format = m_context.getDevice().findDepthFormat().value(),
                        .tiling = vk::ImageTiling::eOptimal,
                        .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal});
  m_depthImageView =
      m_depthImage->getVkImageView({.format = m_context.getDevice().findDepthFormat().value(),
                                    .aspectFlags = vk::ImageAspectFlagBits::eDepth});
}

auto Renderer::createColorResources() -> void
{
  m_colorImage.emplace(m_context.getDevice());
  m_colorImage->create({.width = m_swapchain.getExtent().width,
                        .height = m_swapchain.getExtent().height,
                        .numSamples = m_context.getMaxUsableSampleCount(),
                        .format = m_swapchain.getSurfaceFormat().format,
                        .tiling = vk::ImageTiling::eOptimal,
                        .usage = vk::ImageUsageFlagBits::eColorAttachment,
                        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal});
  m_colorImageView = m_colorImage->getVkImageView({.format = m_swapchain.getSurfaceFormat().format,
                                                   .aspectFlags = vk::ImageAspectFlagBits::eColor});
}

auto Renderer::createDescriptorPool() -> void
{
  std::array<vk::DescriptorPoolSize, 4> poolSize{
      // One descriptor for the camera uniform buffer for each frame in flight.
      // One descriptor for each light storage buffer for each frame in flight.
      // One descriptor for each unique combined image sampler (texture) in the scene.
      // One descriptor for each unique uniform buffer (material) in the scene.
      {
          {.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = m_framesInFlight},
          {.type = vk::DescriptorType::eStorageBuffer,
           .descriptorCount =
               m_framesInFlight *
               static_cast<uint32_t>(3)}, // 3 types of lights: directional, point, spot
          {.type = vk::DescriptorType::eCombinedImageSampler,
           .descriptorCount = static_cast<uint32_t>(m_scene.getObjects().size())},
          {.type = vk::DescriptorType::eUniformBuffer,
           .descriptorCount = static_cast<uint32_t>(m_scene.getObjects().size())},
      }};

  vk::DescriptorPoolCreateInfo poolInfo;
  poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
  poolInfo.maxSets = m_framesInFlight + static_cast<uint32_t>(m_scene.getObjects().size());
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSize.size());
  poolInfo.pPoolSizes = poolSize.data();

  m_descriptorPool = vk::raii::DescriptorPool(m_context.getDevice().logical(), poolInfo);
}

auto Renderer::createDescriptorSetLayouts() -> void
{
  // create one descriptor set layout for the camera uniform buffer that will be bound to
  // binding 1 and the light uniform buffer that will be bound to binding 2, 3 and 4 (directional,
  // point, and spot lights). General Information about the scene will be bound to binding 0.
  {
    vk::DescriptorSetLayoutBinding sceneInfoBinding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment};
    vk::DescriptorSetLayoutBinding cameraBinding{
        .binding = 1,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment};
    vk::DescriptorSetLayoutBinding directionalLightBinding{
        .binding = 2,
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment};
    vk::DescriptorSetLayoutBinding pointLightBinding{
        .binding = 3,
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment};
    vk::DescriptorSetLayoutBinding spotLightBinding{
        .binding = 4,
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment};

    std::array<vk::DescriptorSetLayoutBinding, 5> bindings = {sceneInfoBinding, cameraBinding,
                                                              directionalLightBinding,
                                                              pointLightBinding, spotLightBinding};
    vk::DescriptorSetLayoutCreateInfo layoutInfo{
        .bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data()};

    m_frameDescriptorSetLayout =
        vk::raii::DescriptorSetLayout(m_context.getDevice().logical(), layoutInfo);
  }

  // create one descriptor set layout for all unique textures and materials. Binding 0 will
  // contain all unique textures and binding 1 will contain all unique materials.
  {
    vk::DescriptorSetLayoutBinding textureBinding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = static_cast<uint32_t>(m_uniqueTextures.size()),
        .stageFlags = vk::ShaderStageFlagBits::eFragment};

    vk::DescriptorSetLayoutBinding materialBinding{
        .binding = 1,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = static_cast<uint32_t>(m_uniqueMaterials.size()),
        .stageFlags = vk::ShaderStageFlagBits::eFragment};

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {textureBinding, materialBinding};
    vk::DescriptorSetLayoutCreateInfo layoutInfo{
        .bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data()};

    m_materialDescriptorSetLayout =
        vk::raii::DescriptorSetLayout(m_context.getDevice().logical(), layoutInfo);
  }
}

auto Renderer::createDescriptorSets() -> void
{
  // Create the descriptor sets for the camera for each frame
  // in flight using the frame descriptor set layout.
  {
    std::vector<vk::DescriptorSetLayout> layouts(m_framesInFlight, *m_frameDescriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = *m_descriptorPool,
                                            .descriptorSetCount =
                                                static_cast<uint32_t>(layouts.size()),
                                            .pSetLayouts = layouts.data()};

    m_frameDescriptorSets = m_context.getDevice().logical().allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < m_framesInFlight; i++)
    {
      // Update the descriptor set for the scene information uniform buffer for each frame in
      // flight.
      {
        vk::DescriptorBufferInfo sceneInfoBufferInfo{.buffer =
                                                         m_SceneInfoUniformBuffer[i].getVkBuffer(),
                                                     .offset = 0,
                                                     .range = sizeof(Scene::SceneInfo)};

        vk::WriteDescriptorSet descriptorWrite{.dstSet = m_frameDescriptorSets[i],
                                               .dstBinding = 0,
                                               .dstArrayElement = 0,
                                               .descriptorCount = 1,
                                               .descriptorType = vk::DescriptorType::eUniformBuffer,
                                               .pBufferInfo = &sceneInfoBufferInfo};
        m_context.getDevice().logical().updateDescriptorSets({descriptorWrite}, {});
      }

      // Update the descriptor set for the camera uniform buffer for each frame in flight.
      {
        vk::DescriptorBufferInfo bufferInfo{.buffer = m_cameraUniformBuffers[i].getVkBuffer(),
                                            .offset = 0,
                                            .range = sizeof(vksim::Camera::Params)};

        vk::WriteDescriptorSet descriptorWrite{.dstSet = m_frameDescriptorSets[i],
                                               .dstBinding = 1,
                                               .dstArrayElement = 0,
                                               .descriptorCount = 1,
                                               .descriptorType = vk::DescriptorType::eUniformBuffer,
                                               .pBufferInfo = &bufferInfo};

        m_context.getDevice().logical().updateDescriptorSets({descriptorWrite}, {});
      }

      // Update the descriptor set for the directional light storage buffer for each frame in
      // flight. A fallback storage buffer is always allocated, even when there are no lights.
      {
        vk::DescriptorBufferInfo bufferInfo{
            .buffer = m_directionalLightBuffers[i].getVkBuffer(),
            .offset = 0,
            .range = static_cast<vk::DeviceSize>(
                sizeof(vksim::DirectionalLight::Params) *
                std::max<size_t>(1, m_scene.getDirectionalLights().size()))};

        vk::WriteDescriptorSet descriptorWrite{.dstSet = m_frameDescriptorSets[i],
                                               .dstBinding = 2,
                                               .dstArrayElement = 0,
                                               .descriptorCount = 1,
                                               .descriptorType = vk::DescriptorType::eStorageBuffer,
                                               .pBufferInfo = &bufferInfo};

        m_context.getDevice().logical().updateDescriptorSets({descriptorWrite}, {});
      }

      // Update the descriptor set for the point light storage buffer for each frame in flight.
      // A fallback storage buffer is always allocated, even when there are no lights.
      {
        vk::DescriptorBufferInfo bufferInfo{
            .buffer = m_pointLightBuffers[i].getVkBuffer(),
            .offset = 0,
            .range =
                static_cast<vk::DeviceSize>(sizeof(vksim::PointLight::Params) *
                                            std::max<size_t>(1, m_scene.getPointLights().size()))};

        vk::WriteDescriptorSet descriptorWrite{.dstSet = m_frameDescriptorSets[i],
                                               .dstBinding = 3,
                                               .dstArrayElement = 0,
                                               .descriptorCount = 1,
                                               .descriptorType = vk::DescriptorType::eStorageBuffer,
                                               .pBufferInfo = &bufferInfo};

        m_context.getDevice().logical().updateDescriptorSets({descriptorWrite}, {});
      }

      // Update the descriptor set for the spot light storage buffer for each frame in flight.
      // A fallback storage buffer is always allocated, even when there are no lights.
      {
        vk::DescriptorBufferInfo bufferInfo{
            .buffer = m_spotLightBuffers[i].getVkBuffer(),
            .offset = 0,
            .range =
                static_cast<vk::DeviceSize>(sizeof(vksim::SpotLight::Params) *
                                            std::max<size_t>(1, m_scene.getSpotLights().size()))};

        vk::WriteDescriptorSet descriptorWrite{.dstSet = m_frameDescriptorSets[i],
                                               .dstBinding = 4,
                                               .dstArrayElement = 0,
                                               .descriptorCount = 1,
                                               .descriptorType = vk::DescriptorType::eStorageBuffer,
                                               .pBufferInfo = &bufferInfo};

        m_context.getDevice().logical().updateDescriptorSets({descriptorWrite}, {});
      }
    }
  }

  // Create the descriptor sets for the all unique combined image sampler (texture) and uniform
  // buffer (material) in the scene using the material descriptor set layout.
  {
    vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = *m_descriptorPool,
                                            .descriptorSetCount = static_cast<uint32_t>(1),
                                            .pSetLayouts = &(*m_materialDescriptorSetLayout)};

    m_materialDescriptorSets = m_context.getDevice().logical().allocateDescriptorSets(allocInfo);

    uint32_t textureIndex = 0;
    for (const auto &resourceId : m_uniqueTextures)
    {
      auto texture = m_scene.getResourceManager().GetResource<vksim::Texture>(resourceId);

      vk::DescriptorImageInfo imageInfo{.sampler = texture.value()->getSampler(),
                                        .imageView = texture.value()->getImageView(),
                                        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

      vk::WriteDescriptorSet descriptorWrite{.dstSet = m_materialDescriptorSets[0],
                                             .dstBinding = 0,
                                             .dstArrayElement = textureIndex++,
                                             .descriptorCount = 1,
                                             .descriptorType =
                                                 vk::DescriptorType::eCombinedImageSampler,
                                             .pImageInfo = &imageInfo};
      m_context.getDevice().logical().updateDescriptorSets({descriptorWrite}, {});
    }

    uint32_t materialIndex = 0;
    for (const auto &resourceId : m_uniqueMaterials)
    {
      auto material = m_scene.getResourceManager().GetResource<vksim::Material>(resourceId);

      vk::DescriptorBufferInfo bufferInfo{.buffer = material.value()->getVkBuffer(),
                                          .offset = 0,
                                          .range = sizeof(vksim::MaterialInfo)};

      vk::WriteDescriptorSet descriptorWrite{.dstSet = m_materialDescriptorSets[0],
                                             .dstBinding = 1,
                                             .dstArrayElement = materialIndex++,
                                             .descriptorCount = 1,
                                             .descriptorType = vk::DescriptorType::eUniformBuffer,
                                             .pBufferInfo = &bufferInfo};

      m_context.getDevice().logical().updateDescriptorSets({descriptorWrite}, {});
    }
  }
}

void Renderer::createGraphicsPipeline()
{
  vksim::compiler::SlangCompiler slangCompiler(PROJECT_SOURCE_DIR "/src/shaders");

  auto shaderCodeVert = slangCompiler.compileToSpirv("main.slang", "main", "vertMain");
  if (!shaderCodeVert)
  {
    spdlog::error("{}", shaderCodeVert.error().toString());
    std::abort();
  };

  auto shaderCodeFrag = slangCompiler.compileToSpirv("main.slang", "main", "fragMain");
  if (!shaderCodeFrag)
  {
    spdlog::error("{}", shaderCodeFrag.error().toString());
    std::abort();
  }

  vk::raii::ShaderModule shaderModuleVert = createShaderModule(*shaderCodeVert);
  vk::raii::ShaderModule shaderModuleFrag = createShaderModule(*shaderCodeFrag);

  vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
      .stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModuleVert, .pName = "main"};

  vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
      .stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModuleFrag, .pName = "main"};

  std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {vertShaderStageInfo,
                                                                   fragShaderStageInfo};

  auto bindingDescription = vksim::Vertex::getBindingDescription();
  auto attributeDescriptions = vksim::Vertex::getAttributeDescriptions();
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &bindingDescription,
      .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
      .pVertexAttributeDescriptions = attributeDescriptions.data()};

  vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology =
                                                             vk::PrimitiveTopology::eTriangleList};

  std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport,
                                                 vk::DynamicState::eScissor};

  vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount =
                                                      static_cast<uint32_t>(dynamicStates.size()),
                                                  .pDynamicStates = dynamicStates.data()};

  vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};

  vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable = vk::False,
                                                      .rasterizerDiscardEnable = vk::False,
                                                      .polygonMode = vk::PolygonMode::eFill,
                                                      .cullMode = vk::CullModeFlagBits::eBack,
                                                      .frontFace = vk::FrontFace::eCounterClockwise,
                                                      .depthBiasEnable = vk::False,
                                                      .lineWidth = 1.0F};

  vk::PipelineMultisampleStateCreateInfo multisampling{.rasterizationSamples =
                                                           m_context.getMaxUsableSampleCount(),
                                                       .sampleShadingEnable = vk::False};

  vk::PipelineDepthStencilStateCreateInfo depthStencil{.depthTestEnable = vk::True,
                                                       .depthWriteEnable = vk::True,
                                                       .depthCompareOp = vk::CompareOp::eLess,
                                                       .depthBoundsTestEnable = vk::False,
                                                       .stencilTestEnable = vk::False};

  vk::PipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable = vk::True,
      .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
      .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
      .colorBlendOp = vk::BlendOp::eAdd,
      .srcAlphaBlendFactor = vk::BlendFactor::eOne,
      .dstAlphaBlendFactor = vk::BlendFactor::eZero,
      .alphaBlendOp = vk::BlendOp::eAdd,
      .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

  vk::PipelineColorBlendStateCreateInfo colorBlending{
      .logicOpEnable = vk::False, .attachmentCount = 1, .pAttachments = &colorBlendAttachment};

  // Create the pipeline layout with descriptor set layouts for the camera and material
  // descriptor sets, and a push constant range for the model matrix.
  std::array<vk::DescriptorSetLayout, 2> layouts = {*m_frameDescriptorSetLayout,
                                                    *m_materialDescriptorSetLayout};

  vk::PushConstantRange pushConstantRange{.stageFlags = vk::ShaderStageFlagBits::eVertex |
                                                        vk::ShaderStageFlagBits::eFragment,
                                          .offset = 0,
                                          .size = sizeof(ObjectDescriptor)};

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = layouts.size(),
                                                  .pSetLayouts = layouts.data(),
                                                  .pushConstantRangeCount = 1,
                                                  .pPushConstantRanges = &pushConstantRange};

  m_pipelineLayout = vk::raii::PipelineLayout(m_context.getDevice().logical(), pipelineLayoutInfo);

  vk::GraphicsPipelineCreateInfo graphicsCreateInfo{.stageCount = 2,
                                                    .pStages = shaderStages.data(),
                                                    .pVertexInputState = &vertexInputInfo,
                                                    .pInputAssemblyState = &inputAssembly,
                                                    .pViewportState = &viewportState,
                                                    .pRasterizationState = &rasterizer,
                                                    .pMultisampleState = &multisampling,
                                                    .pDepthStencilState = &depthStencil,
                                                    .pColorBlendState = &colorBlending,
                                                    .pDynamicState = &dynamicState,
                                                    .layout = m_pipelineLayout,
                                                    .renderPass = nullptr};

  vk::PipelineRenderingCreateInfo renderingCreateInfo{
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &m_swapchain.getSurfaceFormat().format,
      .depthAttachmentFormat = m_context.getDevice().findDepthFormat().value()};

  vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo>
      pipelineCreateInfoChain = {graphicsCreateInfo, renderingCreateInfo};

  m_graphicsPipeline =
      vk::raii::Pipeline(m_context.getDevice().logical(), nullptr,
                         pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

[[nodiscard]] auto Renderer::createShaderModule(const std::vector<char> &code) const
    -> vk::raii::ShaderModule
{
  vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(char),
                                        .pCode = reinterpret_cast<const uint32_t *>(code.data())};
  vk::raii::ShaderModule shaderModule{m_context.getDevice().logical(), createInfo};

  return shaderModule;
}

auto Renderer::createCommandBuffers() -> void
{
  m_commandBuffers.clear();

  const auto &commandPool = m_context.getCommandPool(m_queueHandle.familyIndex);

  m_commandBuffers = commandPool.allocateCommandBuffers(
      {.level = vk::CommandBufferLevel::ePrimary, .count = m_framesInFlight});
}

void Renderer::createSyncObjects()
{
  m_imageAvailableSemaphores.clear();
  m_renderFinishedSemaphores.clear();
  m_inFlightFences.clear();

  for (size_t i = 0; i < m_swapchain.getImages().size(); i++)
  {
    m_renderFinishedSemaphores.emplace_back(m_context.getDevice().logical(),
                                            vk::SemaphoreCreateInfo());
  }

  for (size_t i = 0; i < m_framesInFlight; i++)
  {
    m_imageAvailableSemaphores.emplace_back(m_context.getDevice().logical(),
                                            vk::SemaphoreCreateInfo());
    m_inFlightFences.emplace_back(m_context.getDevice().logical(),
                                  vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
  }
}

void Renderer::recordCommandBuffer(uint32_t imageIndex, uint32_t frameIndex,
                                   vk::raii::CommandBuffer &commandBuffer)
{
  commandBuffer.begin({});

  // Before starting rendering, transition the swapchain image to
  // vk::ImageLayout::eColorAttachmentOptimal
  m_swapchain.transitionLayout(
      imageIndex, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, {},
      vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eTopOfPipe,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::ImageAspectFlagBits::eColor,
      commandBuffer);

  // Before starting rendering, transition the multisampling image to
  // vk::ImageLayout::eColorAttachmentOptimal
  m_colorImage->transitionLayout(vk::ImageLayout::eUndefined,
                                 vk::ImageLayout::eColorAttachmentOptimal,
                                 {}, // srcAccessMask (no need to wait for previous operations)
                                 vk::AccessFlagBits2::eColorAttachmentWrite, // dstAccessMask
                                 vk::PipelineStageFlagBits2::eTopOfPipe,     // srcStage
                                 vk::PipelineStageFlagBits2::eColorAttachmentOutput, // dstStage
                                 vk::ImageAspectFlagBits::eColor, commandBuffer);

  // Transition depth image to depth attachment optimal layout
  m_depthImage->transitionLayout(vk::ImageLayout::eUndefined,
                                 vk::ImageLayout::eDepthAttachmentOptimal,
                                 vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                                 vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                                 vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                     vk::PipelineStageFlagBits2::eLateFragmentTests, // srcStage
                                 vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                     vk::PipelineStageFlagBits2::eLateFragmentTests, // dstStage
                                 vk::ImageAspectFlagBits::eDepth, commandBuffer);

  vk::ClearValue clearColor = vk::ClearColorValue(0.0F, 0.0F, 0.0F, 1.0F);
  vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0F, 0.0F);

  vk::RenderingAttachmentInfo colorAttachmentInfo = {
      .imageView = m_colorImageView,
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,

      .resolveMode = vk::ResolveModeFlagBits::eAverage,
      .resolveImageView = m_swapchain.getImageViews()[imageIndex],
      .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,

      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = clearColor};

  vk::RenderingAttachmentInfo depthAttachmentInfo = {.imageView = m_depthImageView,
                                                     .imageLayout =
                                                         vk::ImageLayout::eDepthAttachmentOptimal,
                                                     .loadOp = vk::AttachmentLoadOp::eClear,
                                                     .storeOp = vk::AttachmentStoreOp::eDontCare,
                                                     .clearValue = clearDepth};

  vk::RenderingInfo renderingInfo = {
      .renderArea = {.offset = {.x = 0, .y = 0}, .extent = m_swapchain.getExtent()},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachmentInfo,
      .pDepthAttachment = &depthAttachmentInfo};

  commandBuffer.beginRendering(renderingInfo);

  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);

  commandBuffer.setViewport(
      0, vk::Viewport(0.0F, 0.0F, static_cast<float>(m_swapchain.getExtent().width),
                      static_cast<float>(m_swapchain.getExtent().height), 0.0F, 1.0F));
  commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), m_swapchain.getExtent()));

  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0,
                                   *m_frameDescriptorSets[frameIndex], nullptr);

  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 1,
                                   *m_materialDescriptorSets[0], nullptr);

  for (const auto &object : m_scene.getObjects())
  {
    if (!object->isVisible())
    {
      continue;
    }

    auto *mesh = object->getMesh().value();
    if (mesh == nullptr)
    {
      continue;
    }

    const uint32_t objectId = object->getObjectId();

    // Update model matrix and normal matrix for the current object in the push constant range.
    m_objectDescriptors[objectId].modelMatrix = object->getModelMatrix();
    m_objectDescriptors[objectId].normalMatrix =
        glm::mat4(glm::inverse(glm::transpose(glm::mat3(object->getModelMatrix()))));

    // Push the model matrix and material and texture ids as a push constant to the vertex shader
    commandBuffer.pushConstants(
        m_pipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
        sizeof(ObjectDescriptor), &m_objectDescriptors[objectId]);

    commandBuffer.bindVertexBuffers(0, *mesh->getVertexBuffer(), {0});
    commandBuffer.bindIndexBuffer(*mesh->getIndexBuffer(), 0, vk::IndexType::eUint32);

    commandBuffer.drawIndexed(static_cast<uint32_t>(object->getMesh().value()->getIndexCount()), 1,
                              0, 0, 0);
  }

  commandBuffer.endRendering();

  // After rendering, transition the swapchain image to
  // vk::ImageLayout::ePresentSrcKHR
  /*  m_swapchain.transitionLayout(imageIndex, vk::ImageLayout::eColorAttachmentOptimal,
                                 vk::ImageLayout::ePresentSrcKHR,
                                 vk::AccessFlagBits2::eColorAttachmentWrite,         //
     srcAccessMask
                                 {},                                                 //
     dstAccessMask vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                                 vk::PipelineStageFlagBits2::eBottomOfPipe,          // dstStage
                                 vk::ImageAspectFlagBits::eColor, commandBuffer);
  commandBuffer.end();*/
}

void Renderer::recreateSwapchain()
{
  int width = 0;
  int height = 0;

  m_context.getWindow().getFramebufferSize(width, height);
  while (width == 0 || height == 0)
  {
    m_context.getWindow().getFramebufferSize(width, height);
    vksim::Window::waitEvents();
  }

  m_context.getDevice().logical().waitIdle();

  m_swapchain.recreate();

  m_depthImage->create(
      vksim::ImageCreateInfo{.width = m_swapchain.getExtent().width,
                             .height = m_swapchain.getExtent().height,
                             .numSamples = m_context.getMaxUsableSampleCount(),
                             .format = m_context.getDevice().findDepthFormat().value(),
                             .tiling = vk::ImageTiling::eOptimal,
                             .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                             .properties = vk::MemoryPropertyFlagBits::eDeviceLocal});
  m_depthImageView =
      m_depthImage->getVkImageView({.format = m_context.getDevice().findDepthFormat().value(),
                                    .aspectFlags = vk::ImageAspectFlagBits::eDepth});

  m_colorImage->create(
      vksim::ImageCreateInfo{.width = m_swapchain.getExtent().width,
                             .height = m_swapchain.getExtent().height,
                             .numSamples = m_context.getMaxUsableSampleCount(),
                             .format = m_swapchain.getSurfaceFormat().format,
                             .tiling = vk::ImageTiling::eOptimal,
                             .usage = vk::ImageUsageFlagBits::eColorAttachment,
                             .properties = vk::MemoryPropertyFlagBits::eDeviceLocal});
  m_colorImageView = m_colorImage->getVkImageView({.format = m_swapchain.getSurfaceFormat().format,
                                                   .aspectFlags = vk::ImageAspectFlagBits::eColor});

  auto &camera = m_scene.getCamera();
  camera.transform({.width = m_swapchain.getExtent().width,
                    .height = m_swapchain.getExtent().height,
                    .position = camera.params.cameraPos,
                    .center = camera.m_center,
                    .up = camera.m_up,
                    .fov = camera.m_fov,
                    .nearPlane = camera.m_nearPlane,
                    .farPlane = camera.m_farPlane});

  if (m_imguiRenderer)
  {
    m_imguiRenderer->recreateWithSwapchain();
  }
}

auto Renderer::updateSceneDataForCurrentFrame() -> void
{
  // Update the scene information uniform buffer for the current frame.
  std::memcpy(m_SceneInfoUniformBufferMapped[m_currentFrame], &m_scene.getSceneInfo(),
              sizeof(Scene::SceneInfo));

  // Update the camera uniform buffer for the current frame.
  std::memcpy(m_cameraUniformBuffersMapped[m_currentFrame], &m_scene.getCamera().params,
              sizeof(Camera::Params));

  // Update Directional lights
  {
    uint32_t lightIndex = 0;
    auto *dst = static_cast<char *>(m_directionalLightBuffersMapped[m_currentFrame]);
    for (const auto &light : m_scene.getDirectionalLights())
    {
      std::memcpy(dst + (lightIndex * sizeof(DirectionalLight::Params)), &light->params,
                  sizeof(DirectionalLight::Params));
      ++lightIndex;
    }
  }

  // Update Point lights
  {
    uint32_t lightIndex = 0;
    auto *dst = static_cast<char *>(m_pointLightBuffersMapped[m_currentFrame]);
    for (const auto &light : m_scene.getPointLights())
    {
      std::memcpy(dst + (lightIndex * sizeof(PointLight::Params)), &light->params,
                  sizeof(PointLight::Params));
      ++lightIndex;
    }
  }

  // Update Spot lights
  {
    uint32_t lightIndex = 0;
    auto *dst = static_cast<char *>(m_spotLightBuffersMapped[m_currentFrame]);
    for (const auto &light : m_scene.getSpotLights())
    {

      std::memcpy(dst + (lightIndex * sizeof(SpotLight::Params)), &light->params,
                  sizeof(SpotLight::Params));
      ++lightIndex;
    }
  }
}

auto Renderer::drawFrame() -> void
{
  updateSceneDataForCurrentFrame();

  // Get current frame's synchronization objects and command buffer
  auto &inFlightFence = m_inFlightFences[m_currentFrame];
  auto &presentCompleteSemaphore = m_imageAvailableSemaphores[m_currentFrame];
  auto &commandBuffer = m_commandBuffers[m_currentFrame];

  // Cpu wait for the previous graphics queue submission to finish
  // before submitting the next one within the same frame.
  auto fenceResult =
      m_context.getDevice().logical().waitForFences(*inFlightFence, vk::True, UINT64_MAX);
  if (fenceResult != vk::Result::eSuccess)
  {
    throw std::runtime_error("failed to wait for fence!");
  }

  // Acquire the next image from the swapchain and recreate the
  // swapchain if it is out of date.
  auto [result, imageIndex] =
      m_swapchain.get().acquireNextImage(UINT64_MAX, *presentCompleteSemaphore, nullptr);

  if (result == vk::Result::eErrorOutOfDateKHR)
  {
    recreateSwapchain();
    return;
  }
  if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
  {
    assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  // Reset the fence to the unsignaled state for the next frame
  m_context.getDevice().logical().resetFences(*inFlightFence);

  // Get the semaphore to signal when rendering is finished for this
  // image
  auto &renderFinishedSemaphore = m_renderFinishedSemaphores[imageIndex];

  // Record the command buffer for the acquired image
  m_commandBuffers[m_currentFrame].reset();
  recordCommandBuffer(imageIndex, m_currentFrame, m_commandBuffers[m_currentFrame]);

  // Update and record the ImGui command buffer for the current frame
  m_imguiRenderer->update();
  m_imguiRenderer->recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex);

  vk::PipelineStageFlags waitDestinationStageMask(
      vk::PipelineStageFlagBits::eColorAttachmentOutput);
  const vk::SubmitInfo submitInfo{.waitSemaphoreCount = 1,
                                  .pWaitSemaphores = &*presentCompleteSemaphore,
                                  .pWaitDstStageMask = &waitDestinationStageMask,
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &*m_commandBuffers[m_currentFrame],
                                  .signalSemaphoreCount = 1,
                                  .pSignalSemaphores = &*renderFinishedSemaphore};

  const auto &graphicsQueue = m_context.getDefaultQueue();
  graphicsQueue.queue.submit(submitInfo, *inFlightFence);

  const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1,
                                          .pWaitSemaphores = &*renderFinishedSemaphore,
                                          .swapchainCount = 1,
                                          .pSwapchains = &*m_swapchain.get(),
                                          .pImageIndices = &imageIndex};

  result = graphicsQueue.queue.presentKHR(presentInfoKHR);
  if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR))
  {
    recreateSwapchain();
  }
  else
  {
    assert(result == vk::Result::eSuccess);
  }

  m_currentFrame = (m_currentFrame + 1) % m_framesInFlight;
}
} // namespace vksim