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
#include "vksim/core/camera/Camera.hpp"
#include "vksim/core/commands/CommandPool.hpp"
#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/resources/Mesh.hpp"
#include "vksim/core/resources/ResourceManager.hpp"
#include "vksim/core/resources/Texture.hpp"
#include "vksim/core/swapchain/Swapchain.hpp"
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

class HelloTriangleApplication
{
public:
  void run()
  {
    initWindow();

    context = vksim::VulkanContext(
        window, {.instance = {.appName = "Hello Triangle",
                              .appVersion = VK_MAKE_VERSION(1, 0, 0),
                              .engineName = "No Engine",
                              .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                              .apiVersion = VK_API_VERSION_1_3,
                              .layers = {"VK_LAYER_KHRONOS_validation"},
                              .extensions = {vk::KHRPortabilityEnumerationExtensionName}},

                 .device = {
                     .extensions = {vk::KHRSwapchainExtensionName, "VK_KHR_portability_subset"},
                     .features = {.anisotropicFiltering = true,
                                  .shaderDrawParameters = true,
                                  .dynamicRendering = true,
                                  .synchronization2 = true,
                                  .extendedDynamicState = true},
                 }});

    auto &graphicsQueue = context.requestQueue({.requiredFlags = vk::QueueFlagBits::eGraphics |
                                                                 vk::QueueFlagBits::eCompute |
                                                                 vk::QueueFlagBits::eTransfer,
                                                .requiresPresent = true});
    context.build();

    auto &commandPool = context.getCommandPool(graphicsQueue.familyIndex);

    commandBuffers = commandPool.allocateCommandBuffers(
        {.level = vk::CommandBufferLevel::ePrimary, .count = MAX_FRAMES_IN_FLIGHT});

    camera = std::move(vksim::Camera(&context, MAX_FRAMES_IN_FLIGHT));
    camera.update({.width = swapchain.getExtent().width,
                   .height = swapchain.getExtent().height,
                   .position = glm::vec3(1.0F, 1.0F, 1.5F),
                   .center = glm::vec3(0.0F, 0.0F, 0.0F),
                   .up = glm::vec3(0.0F, 0.0F, 1.0F),
                   .fov = 90.0F,
                   .nearPlane = 0.1F,
                   .farPlane = 10.0F});

    swapchain = vksim::Swapchain(&context, {.window = window,
                                            .format = vk::Format::eB8G8R8A8Srgb,
                                            .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
                                            .imageCount = 3});

    depthImage = std::move(vksim::Image(
        &context, vksim::ImageCreateInfo{.width = swapchain.getExtent().width,
                                         .height = swapchain.getExtent().height,
                                         .numSamples = context.getMaxUsableSampleCount(),
                                         .format = vksim::Image::findDepthFormat(&context).value(),
                                         .tiling = vk::ImageTiling::eOptimal,
                                         .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                                         .properties = vk::MemoryPropertyFlagBits::eDeviceLocal}));

    depthImageView =
        depthImage.getVkImageView({.format = vksim::Image::findDepthFormat(&context).value(),
                                   .aspectFlags = vk::ImageAspectFlagBits::eDepth});

    colorImage = std::move(vksim::Image(
        &context, vksim::ImageCreateInfo{.width = swapchain.getExtent().width,
                                         .height = swapchain.getExtent().height,
                                         .numSamples = context.getMaxUsableSampleCount(),
                                         .format = swapchain.getSurfaceFormat().format,
                                         .tiling = vk::ImageTiling::eOptimal,
                                         .usage = vk::ImageUsageFlagBits::eColorAttachment,
                                         .properties = vk::MemoryPropertyFlagBits::eDeviceLocal}));

    colorImageView = colorImage.getVkImageView({.format = swapchain.getSurfaceFormat().format,
                                                .aspectFlags = vk::ImageAspectFlagBits::eColor});

    vksim::UploadContext uploadContext(&context);
    uploadContext.begin();

    resourceManager.Load<vksim::Mesh>("viking_mesh",
                                      PROJECT_SOURCE_DIR "/assets/meshes/viking_room.obj", &context,
                                      uploadContext);

    resourceManager.Load<vksim::Texture>("viking_texture",
                                         PROJECT_SOURCE_DIR "/assets/textures/viking_room.png",
                                         &context, uploadContext);

    uploadContext.submitAndWait();

    assert(graphicsQueue.queue != nullptr && "Graphics queue is null after context build.");

    initVulkan();
    mainLoop();
    cleanup();
  }

private:
  void initWindow()
  {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
  }

  static void framebufferResizeCallback(GLFWwindow *window, int width, int height)
  {
    auto *app = reinterpret_cast<HelloTriangleApplication *>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
  }

  void initVulkan()
  {
    createDescriptorPool();
    createDescriptorSetLayout();
    createDescriptorSets();
    createGraphicsPipeline();
    createSyncObjects();
  }

  void mainLoop()
  {
    while (glfwWindowShouldClose(window) == 0)
    {
      glfwPollEvents();

      drawFrame();
    }
    context.getDevice().waitIdle();
  }

  void createDescriptorSetLayout()
  {
    std::array<vk::DescriptorSetLayoutBinding, 2> bindings{
        {{.binding = 0,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eVertex},
         {.binding = 1,
          .descriptorType = vk::DescriptorType::eCombinedImageSampler,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eFragment}}};

    vk::DescriptorSetLayoutCreateInfo layoutInfo{
        .bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data()};

    descriptorSetLayout = vk::raii::DescriptorSetLayout(context.getDevice(), layoutInfo);
  }

  void createDescriptorSets()
  {
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = descriptorPool,
                                            .descriptorSetCount =
                                                static_cast<uint32_t>(layouts.size()),
                                            .pSetLayouts = layouts.data()};

    descriptorSets = context.getDevice().allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      vk::DescriptorBufferInfo bufferInfo{.buffer = camera.getUniformBuffer(i),
                                          .offset = 0,
                                          .range = sizeof(vksim::CameraUniformBufferObject)};
      vk::DescriptorImageInfo imageInfo{
          .sampler =
              resourceManager.GetResource<vksim::Texture>("viking_texture").value()->getSampler(),
          .imageView =
              resourceManager.GetResource<vksim::Texture>("viking_texture").value()->getImageView(),
          .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

      std::array<vk::WriteDescriptorSet, 2> descriptorWrites{
          {{.dstSet = descriptorSets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &bufferInfo},
           {.dstSet = descriptorSets[i],
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &imageInfo}}};
      context.getDevice().updateDescriptorSets(descriptorWrites, {});
    }
  }

  void createDescriptorPool()
  {
    std::array<vk::DescriptorPoolSize, 2> poolSize{
        {{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = MAX_FRAMES_IN_FLIGHT},
         {.type = vk::DescriptorType::eCombinedImageSampler,
          .descriptorCount = MAX_FRAMES_IN_FLIGHT}}};

    vk::DescriptorPoolCreateInfo poolInfo{.flags =
                                              vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                                          .maxSets = MAX_FRAMES_IN_FLIGHT,
                                          .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
                                          .pPoolSizes = poolSize.data()};

    descriptorPool = vk::raii::DescriptorPool(context.getDevice(), poolInfo);
  }

  void recreateSwapChain()
  {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0)
    {
      glfwGetFramebufferSize(window, &width, &height);
      glfwWaitEvents();
    }

    context.getDevice().waitIdle();

    swapchain.recreate();

    depthImage = std::move(vksim::Image(
        &context, vksim::ImageCreateInfo{.width = swapchain.getExtent().width,
                                         .height = swapchain.getExtent().height,
                                         .numSamples = context.getMaxUsableSampleCount(),
                                         .format = vksim::Image::findDepthFormat(&context).value(),
                                         .tiling = vk::ImageTiling::eOptimal,
                                         .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                                         .properties = vk::MemoryPropertyFlagBits::eDeviceLocal}));
    depthImageView =
        depthImage.getVkImageView({.format = vksim::Image::findDepthFormat(&context).value(),
                                   .aspectFlags = vk::ImageAspectFlagBits::eDepth});

    colorImage = std::move(vksim::Image(
        &context, vksim::ImageCreateInfo{.width = swapchain.getExtent().width,
                                         .height = swapchain.getExtent().height,
                                         .numSamples = context.getMaxUsableSampleCount(),
                                         .format = swapchain.getSurfaceFormat().format,
                                         .tiling = vk::ImageTiling::eOptimal,
                                         .usage = vk::ImageUsageFlagBits::eColorAttachment,
                                         .properties = vk::MemoryPropertyFlagBits::eDeviceLocal}));
    colorImageView = colorImage.getVkImageView({.format = swapchain.getSurfaceFormat().format,
                                                .aspectFlags = vk::ImageAspectFlagBits::eColor});
  }

  void drawFrame()
  {
    // Get current frame's synchronization objects and command buffer
    auto &inFlightFence = inFlightFences[frameIndex];
    auto &presentCompleteSemaphore = presentCompleteSemaphores[frameIndex];
    auto &commandBuffer = commandBuffers[frameIndex];

    // Cpu wait for the previous graphics queue submission to finish
    // before submitting the next one within the same frame.
    auto fenceResult = context.getDevice().waitForFences(*inFlightFence, vk::True, UINT64_MAX);
    if (fenceResult != vk::Result::eSuccess)
    {
      throw std::runtime_error("failed to wait for fence!");
    }

    // Acquire the next image from the swapchain and recreate the
    // swapchain if it is out of date.
    auto [result, imageIndex] =
        swapchain.get().acquireNextImage(UINT64_MAX, *presentCompleteSemaphore, nullptr);

    if (result == vk::Result::eErrorOutOfDateKHR)
    {
      recreateSwapChain();
      return;
    }
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
    {
      assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
      throw std::runtime_error("failed to acquire swap chain image!");
    }

    // Reset the fence to the unsignaled state for the next frame
    context.getDevice().resetFences(*inFlightFence);

    // Update the uniform buffer for the current frame
    camera.update(
        {
            .width = swapchain.getExtent().width,
            .height = swapchain.getExtent().height,
            .position = glm::vec3(1.0F, 1.0F, 1.5F * static_cast<float>(std::sin(glfwGetTime()))),
        },
        frameIndex);

    // Get the semaphore to signal when rendering is finished for this
    // image
    auto &renderFinishedSemaphore = renderFinishedSemaphores[imageIndex];

    // Record the command buffer for the acquired image
    commandBuffers[frameIndex].reset();
    recordCommandBuffer(imageIndex, commandBuffers[frameIndex]);

    vk::PipelineStageFlags waitDestinationStageMask(
        vk::PipelineStageFlagBits::eColorAttachmentOutput);
    const vk::SubmitInfo submitInfo{.waitSemaphoreCount = 1,
                                    .pWaitSemaphores = &*presentCompleteSemaphore,
                                    .pWaitDstStageMask = &waitDestinationStageMask,
                                    .commandBufferCount = 1,
                                    .pCommandBuffers = &*commandBuffer,
                                    .signalSemaphoreCount = 1,
                                    .pSignalSemaphores = &*renderFinishedSemaphore};

    const auto &graphicsQueue = context.getDefaultQueue();
    graphicsQueue.queue.submit(submitInfo, *inFlightFence);

    const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1,
                                            .pWaitSemaphores = &*renderFinishedSemaphore,
                                            .swapchainCount = 1,
                                            .pSwapchains = &*swapchain.get(),
                                            .pImageIndices = &imageIndex};

    result = graphicsQueue.queue.presentKHR(presentInfoKHR);
    if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) ||
        framebufferResized)
    {
      recreateSwapChain();
      framebufferResized = false;
    }
    else
    {
      assert(result == vk::Result::eSuccess);
    }

    frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
  }

  void recordCommandBuffer(uint32_t imageIndex, vk::raii::CommandBuffer &commandBuffer)
  {
    commandBuffer.begin({});

    // Before starting rendering, transition the swapchain image to
    // vk::ImageLayout::eColorAttachmentOptimal
    swapchain.transitionLayout(
        imageIndex, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, {},
        vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eTopOfPipe,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::ImageAspectFlagBits::eColor,
        commandBuffer);

    // Before starting rendering, transition the multisampling image to
    // vk::ImageLayout::eColorAttachmentOptimal
    colorImage.transitionLayout(vk::ImageLayout::eUndefined,
                                vk::ImageLayout::eColorAttachmentOptimal,
                                {}, // srcAccessMask (no need to wait for previous operations)
                                vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
                                vk::PipelineStageFlagBits2::eTopOfPipe,             // srcStage
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput, // dstStage
                                vk::ImageAspectFlagBits::eColor, commandBuffer);

    // Transition depth image to depth attachment optimal layout
    depthImage.transitionLayout(vk::ImageLayout::eUndefined,
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
        .imageView = colorImageView,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,

        .resolveMode = vk::ResolveModeFlagBits::eAverage,
        .resolveImageView = swapchain.getImageViews()[imageIndex],
        .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,

        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clearColor};

    vk::RenderingAttachmentInfo depthAttachmentInfo = {.imageView = depthImageView,
                                                       .imageLayout =
                                                           vk::ImageLayout::eDepthAttachmentOptimal,
                                                       .loadOp = vk::AttachmentLoadOp::eClear,
                                                       .storeOp = vk::AttachmentStoreOp::eDontCare,
                                                       .clearValue = clearDepth};

    vk::RenderingInfo renderingInfo = {
        .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapchain.getExtent()},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfo,
        .pDepthAttachment = &depthAttachmentInfo};

    commandBuffer.beginRendering(renderingInfo);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

    commandBuffer.setViewport(
        0, vk::Viewport(0.0F, 0.0F, static_cast<float>(swapchain.getExtent().width),
                        static_cast<float>(swapchain.getExtent().height), 0.0F, 1.0F));
    commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchain.getExtent()));

    commandBuffer.bindVertexBuffers(
        0, *resourceManager.GetResource<vksim::Mesh>("viking_mesh").value()->getVertexBuffer(),
        {0});
    commandBuffer.bindIndexBuffer(
        *resourceManager.GetResource<vksim::Mesh>("viking_mesh").value()->getIndexBuffer(), 0,
        vk::IndexType::eUint32);
    assert(*pipelineLayout != VK_NULL_HANDLE);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0,
                                     *descriptorSets[frameIndex], nullptr);
    commandBuffer.drawIndexed(
        static_cast<uint32_t>(
            resourceManager.GetResource<vksim::Mesh>("viking_mesh").value()->getIndexCount()),
        1, 0, 0, 0);

    commandBuffer.endRendering();

    // After rendering, transition the swapchain image to
    // vk::ImageLayout::ePresentSrcKHR
    swapchain.transitionLayout(imageIndex, vk::ImageLayout::eColorAttachmentOptimal,
                               vk::ImageLayout::ePresentSrcKHR,
                               vk::AccessFlagBits2::eColorAttachmentWrite,         // srcAccessMask
                               {},                                                 // dstAccessMask
                               vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                               vk::PipelineStageFlagBits2::eBottomOfPipe,          // dstStage
                               vk::ImageAspectFlagBits::eColor, commandBuffer);
    commandBuffer.end();
  }

  void createSyncObjects()
  {
    assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() &&
           inFlightFences.empty());

    for (size_t i = 0; i < swapchain.getImages().size(); i++)
    {
      renderFinishedSemaphores.emplace_back(context.getDevice(), vk::SemaphoreCreateInfo());
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      presentCompleteSemaphores.emplace_back(context.getDevice(), vk::SemaphoreCreateInfo());
      inFlightFences.emplace_back(context.getDevice(),
                                  vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
    }
  }

  void createGraphicsPipeline()
  {
    auto shaderCodeVert = slangCompiler.compileToSpirv(
        PROJECT_SOURCE_DIR "/src/shaders/shader.slang", "shader", "vertMain");
    if (!shaderCodeVert)
    {
      spdlog::error("{}", shaderCodeVert.error().toString());
      std::abort();
    };

    auto shaderCodeFrag = slangCompiler.compileToSpirv(
        PROJECT_SOURCE_DIR "/src/shaders/shader.slang", "shader", "fragMain");
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

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology = vk::PrimitiveTopology::eTriangleList};

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
                                                        .frontFace =
                                                            vk::FrontFace::eCounterClockwise,
                                                        .depthBiasEnable = vk::False,
                                                        .lineWidth = 1.0F};

    vk::PipelineMultisampleStateCreateInfo multisampling{.rasterizationSamples =
                                                             context.getMaxUsableSampleCount(),
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

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount = 1, .pSetLayouts = &*descriptorSetLayout, .pushConstantRangeCount = 0};

    pipelineLayout = vk::raii::PipelineLayout(context.getDevice(), pipelineLayoutInfo);

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
                                                      .layout = pipelineLayout,
                                                      .renderPass = nullptr};

    vk::PipelineRenderingCreateInfo renderingCreateInfo{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchain.getSurfaceFormat().format,
        .depthAttachmentFormat = vksim::Image::findDepthFormat(&context).value()};

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo>
        pipelineCreateInfoChain = {graphicsCreateInfo, renderingCreateInfo};

    graphicsPipeline =
        vk::raii::Pipeline(context.getDevice(), nullptr,
                           pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
  }

  [[nodiscard]] auto createShaderModule(const std::vector<char> &code) const
      -> vk::raii::ShaderModule
  {
    vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(char),
                                          .pCode = reinterpret_cast<const uint32_t *>(code.data())};
    vk::raii::ShaderModule shaderModule{context.getDevice(), createInfo};

    return shaderModule;
  }

  void cleanup()
  {
    glfwDestroyWindow(window);
    glfwTerminate();
  }

  GLFWwindow *window;

  vksim::compiler::SlangCompiler slangCompiler;

  vksim::VulkanContext context;
  vksim::Swapchain swapchain;
  vksim::ResourceManager resourceManager;

  std::vector<vk::raii::CommandBuffer> commandBuffers;

  vksim::Camera camera;

  vksim::Image depthImage;
  vksim::Image colorImage;
  vk::raii::ImageView depthImageView = nullptr;
  vk::raii::ImageView colorImageView = nullptr;

  vk::raii::Pipeline graphicsPipeline = nullptr;

  std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
  std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
  std::vector<vk::raii::Fence> inFlightFences;

  vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
  vk::raii::PipelineLayout pipelineLayout = nullptr;

  bool framebufferResized = false;

  uint32_t frameIndex = 0;
  /*
    const std::vector<Vertex> vertices = {
        // Apex
        {.pos = {0.0F, 0.0F, 1.0F}, .color = {0.0F, 1.0F, 1.0F}}, // 0

        // Base
        {.pos = {-0.5F, -0.5F, 0.F}, .color = {0.0F, 1.0F, 1.0F}}, // 1
        {.pos = {0.5F, -0.5F, 0.F}, .color = {1.0F, 0.0F, 1.0F}},  // 2
        {.pos = {0.5F, 0.5F, 0.F}, .color = {1.0F, 1.0F, 0.0F}},   // 3
        {.pos = {-0.5F, 0.5F, 0.F}, .color = {1.0F, 0.0F, 1.0F}},  // 4
    };

    const std::vector<uint32_t> indices = {
        // Side faces
        0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1,
        // Base
        1, 4, 3, 1, 3, 2};

  const std::vector<Vertex> vertices = {{.pos = {-0.5F, -0.5F, 0.0},
                                         .color = {1.0F, 0.0F, 0.0F},
                                         .uv = {1.0F, 0.0F}},
                                        {.pos = {0.5F, -0.5F, 0.0},
                                         .color = {0.0F, 1.0F, 0.0F},
                                         .uv = {0.0F, 0.0F}},
                                        {.pos = {0.5F, 0.5F, 0.0},
                                         .color = {0.0F, 0.0F, 1.0F},
                                         .uv = {0.0F, 1.0F}},
                                        {.pos = {-0.5F, 0.5F, 0.0},
                                         .color = {1.0F, 1.0F, 1.0F},
                                         .uv = {1.0F, 1.0F}},

                                        {.pos = {-0.5F, -0.5F, -0.5F},
                                         .color = {1.0F, 0.0F, 0.0F},
                                         .uv = {2.0F, 0.0F}},
                                        {.pos = {0.5F, -0.5F, -0.5F},
                                         .color = {0.0F, 1.0F, 0.0F},
                                         .uv = {0.0F, 0.0F}},
                                        {.pos = {0.5F, 0.5F, -0.5F},
                                         .color = {0.0F, 0.0F, 1.0F},
                                         .uv = {0.0F, 2.0F}},
                                        {.pos = {-0.5F, 0.5F, -0.5F},
                                         .color = {1.0F, 1.0F, 1.0F},
                                         .uv = {2.0F, 2.0F}}};

  const std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0,
                                         4, 5, 6, 6, 7, 4};
*/

  vk::raii::DescriptorPool descriptorPool = nullptr;
  std::vector<vk::raii::DescriptorSet> descriptorSets;
};

auto main() -> int
{
  vksim::logging::set_runtime_log_level();
  try
  {
    HelloTriangleApplication app;
    app.run();
  }
  catch (const std::exception &e)
  {
    std::cerr << e.what() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
