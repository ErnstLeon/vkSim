#include "vksim/core/render/Renderer.hpp"
#include "vksim/core/render/Swapchain.hpp"
#include "vksim/slang/SlangCompiler.hpp"

namespace vksim
{
Renderer::Renderer(VulkanContext &context, Scene &scene, QueueHandle &queueHandle,
                   uint32_t framesInFlight)
    : m_context(context), m_scene(scene), m_queueHandle(queueHandle),
      m_framesInFlight(framesInFlight), m_swapchain(context)
{
  createCameraResources();
  createSwapchain();
  createDepthResources();
  createColorResources();
  createDescriptorPool();
  createDescriptorSetLayouts();
  createDescriptorSets();
  createGraphicsPipeline();
  createCommandBuffers();
  createSyncObjects();
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
        BufferCreateInfo{.size = sizeof(CameraUniformBufferObject),
                         .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                         .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                       vk::MemoryPropertyFlagBits::eHostCoherent});
    m_cameraUniformBuffersMapped.emplace_back(
        m_cameraUniformBuffers.back().getVkBufferMemory().mapMemory(
            0, sizeof(CameraUniformBufferObject)));
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
  std::array<vk::DescriptorPoolSize, 2> poolSize{
      {{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = m_framesInFlight},
       {.type = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = static_cast<uint32_t>(m_scene.getObjects().size())}}};

  vk::DescriptorPoolCreateInfo poolInfo;
  poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
  poolInfo.maxSets = m_framesInFlight + static_cast<uint32_t>(m_scene.getObjects().size());
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSize.size());
  poolInfo.pPoolSizes = poolSize.data();

  m_descriptorPool = vk::raii::DescriptorPool(m_context.getDevice().logical(), poolInfo);
}

auto Renderer::createDescriptorSetLayouts() -> void
{
  // Define the descriptor set layout binding for the uniform camera buffer
  {
    vk::DescriptorSetLayoutBinding binding{.binding = 0,
                                           .descriptorType = vk::DescriptorType::eUniformBuffer,
                                           .descriptorCount = 1,
                                           .stageFlags = vk::ShaderStageFlagBits::eVertex};

    vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = 1, .pBindings = &binding};

    m_globalDescriptorSetLayout =
        vk::raii::DescriptorSetLayout(m_context.getDevice().logical(), layoutInfo);
  }

  // Define the descriptor set layout binding for the combined image sampler
  {
    vk::DescriptorSetLayoutBinding binding{.binding = 0,
                                           .descriptorType =
                                               vk::DescriptorType::eCombinedImageSampler,
                                           .descriptorCount = 1,
                                           .stageFlags = vk::ShaderStageFlagBits::eFragment};

    vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = 1, .pBindings = &binding};

    m_materialDescriptorSetLayout =
        vk::raii::DescriptorSetLayout(m_context.getDevice().logical(), layoutInfo);
  }
}

auto Renderer::createDescriptorSets() -> void
{
  // Create the descriptor sets for the camera/global uniform buffer
  // Allocate descriptor sets for each frame in flight using the global descriptor set layout
  {
    std::vector<vk::DescriptorSetLayout> layouts(m_framesInFlight, *m_globalDescriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = *m_descriptorPool,
                                            .descriptorSetCount =
                                                static_cast<uint32_t>(layouts.size()),
                                            .pSetLayouts = layouts.data()};

    m_globalDescriptorSets = m_context.getDevice().logical().allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < m_framesInFlight; i++)
    {
      vk::DescriptorBufferInfo bufferInfo{.buffer = m_cameraUniformBuffers[i].getVkBuffer(),
                                          .offset = 0,
                                          .range = sizeof(vksim::CameraUniformBufferObject)};

      vk::WriteDescriptorSet descriptorWrite{.dstSet = m_globalDescriptorSets[i],
                                             .dstBinding = 0,
                                             .dstArrayElement = 0,
                                             .descriptorCount = 1,
                                             .descriptorType = vk::DescriptorType::eUniformBuffer,
                                             .pBufferInfo = &bufferInfo};
      m_context.getDevice().logical().updateDescriptorSets({descriptorWrite}, {});
    }
  }

  // Create the descriptor sets for the combined image sampler (texture) for each object in the
  // scene. Only create descriptor sets for objects that have a texture assigned.
  {
    std::vector<vk::DescriptorSetLayout> layouts(m_scene.getObjects().size(),
                                                 *m_materialDescriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = *m_descriptorPool,
                                            .descriptorSetCount =
                                                static_cast<uint32_t>(layouts.size()),
                                            .pSetLayouts = layouts.data()};

    m_materialDescriptorSets = m_context.getDevice().logical().allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < m_scene.getObjects().size(); i++)
    {
      auto texture = m_scene.getObjects()[i]->getTexture();
      if (!texture.has_value())
      {
        spdlog::warn("Scene object {} has no texture assigned, skipping descriptor set creation",
                     i);
        continue;
      }
      if (!m_scene.getObjects()[i]->isVisible())
      {
        spdlog::warn("Scene object {} is not visible, skipping descriptor set creation", i);
        continue;
      }

      vk::DescriptorImageInfo imageInfo{.sampler = texture.value()->getSampler(),
                                        .imageView = texture.value()->getImageView(),
                                        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

      vk::WriteDescriptorSet descriptorWrite{.dstSet = m_materialDescriptorSets[i],
                                             .dstBinding = 0,
                                             .dstArrayElement = 0,
                                             .descriptorCount = 1,
                                             .descriptorType =
                                                 vk::DescriptorType::eCombinedImageSampler,
                                             .pImageInfo = &imageInfo};
      m_context.getDevice().logical().updateDescriptorSets({descriptorWrite}, {});
    }
  }
}

void Renderer::createGraphicsPipeline()
{
  vksim::compiler::SlangCompiler slangCompiler;

  auto shaderCodeVert = slangCompiler.compileToSpirv(PROJECT_SOURCE_DIR "/src/shaders/shader.slang",
                                                     "shader", "vertMain");
  if (!shaderCodeVert)
  {
    spdlog::error("{}", shaderCodeVert.error().toString());
    std::abort();
  };

  auto shaderCodeFrag = slangCompiler.compileToSpirv(PROJECT_SOURCE_DIR "/src/shaders/shader.slang",
                                                     "shader", "fragMain");
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
  std::array<vk::DescriptorSetLayout, 2> layouts = {*m_globalDescriptorSetLayout,
                                                    *m_materialDescriptorSetLayout};

  vk::PushConstantRange pushConstantRange{
      .stageFlags = vk::ShaderStageFlagBits::eVertex, .offset = 0, .size = sizeof(glm::mat4)};

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

  for (const auto &object : m_scene.getObjects())
  {
    if (!object->isVisible() || !object->getMesh().has_value())
    {
      continue;
    }

    commandBuffer.bindVertexBuffers(0, *object->getMesh().value()->getVertexBuffer(), {0});
    commandBuffer.bindIndexBuffer(*object->getMesh().value()->getIndexBuffer(), 0,
                                  vk::IndexType::eUint32);

    // Push the model matrix as a push constant to the vertex shader
    commandBuffer.pushConstants(m_pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0,
                                sizeof(glm::mat4), &object->getModelMatrix());

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0,
                                     *m_globalDescriptorSets[frameIndex], nullptr);

    if (object->getTexture().has_value())
    {
      commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 1,
                                       *m_materialDescriptorSets[object->getObjectId()], nullptr);
    }

    commandBuffer.drawIndexed(static_cast<uint32_t>(object->getMesh().value()->getIndexCount()), 1,
                              0, 0, 0);
  }

  commandBuffer.endRendering();

  // After rendering, transition the swapchain image to
  // vk::ImageLayout::ePresentSrcKHR
  m_swapchain.transitionLayout(imageIndex, vk::ImageLayout::eColorAttachmentOptimal,
                               vk::ImageLayout::ePresentSrcKHR,
                               vk::AccessFlagBits2::eColorAttachmentWrite,         // srcAccessMask
                               {},                                                 // dstAccessMask
                               vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                               vk::PipelineStageFlagBits2::eBottomOfPipe,          // dstStage
                               vk::ImageAspectFlagBits::eColor, commandBuffer);
  commandBuffer.end();
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
}

auto Renderer::drawFrame() -> void
{
  // Update the camera uniform buffer for the current frame
  CameraUniformBufferObject ubo{};
  ubo.view = m_scene.getCamera().getViewMatrix();
  ubo.proj = m_scene.getCamera().getProjectionMatrix();
  std::memcpy(m_cameraUniformBuffersMapped[m_currentFrame], &ubo, sizeof(ubo));

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