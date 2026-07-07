#pragma once

#include "vksim/core/buffers/Image.hpp"
#include <deque>
#include <unordered_map>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/render/Swapchain.hpp"
#include "vksim/core/scene/Scene.hpp"

namespace vksim
{

/** @brief Renderer class encapsulates the Vulkan rendering pipeline and manages
 *        rendering resources.
 * @note The Renderer class is responsible for creating and managing the Vulkan swapchain, depth and
 * color resources, descriptor sets, graphics pipeline, command buffers, and synchronization
 * objects. It provides a drawFrame() method to render a frame and handles swapchain recreation when
 * necessary.
 */
class Renderer
{
public:
  /** @brief Constructs a Renderer with the specified Vulkan context, scene, and queue handle.
   * @param context The Vulkan context.
   * @param scene The scene to be rendered.
   * @param queueHandle The handle to the Vulkan queue.
   * @param framesInFlight The number of frames that can be processed concurrently.
   */
  Renderer(VulkanContext &context, Scene &scene, QueueHandle &queueHandle,
           uint32_t framesInFlight = 1);

  /** @brief Renders a frame. */
  auto drawFrame() -> void;
  [[nodiscard]] auto getExtent() const -> vk::Extent2D { return m_swapchain.getExtent(); }

private:
  auto createDescriptorPool() -> void;
  auto createDescriptorSetLayouts() -> void;
  auto createDescriptorSets() -> void;
  auto createCommandBuffers() -> void;
  auto recordCommandBuffer(uint32_t imageIndex, uint32_t frameIndex,
                           vk::raii::CommandBuffer &commandBuffer) -> void;
  auto createSwapchain() -> void;
  auto recreateSwapchain() -> void;
  auto createSyncObjects() -> void;
  auto createCameraResources() -> void;
  auto createDepthResources() -> void;
  auto createColorResources() -> void;
  auto createGraphicsPipeline() -> void;

  [[nodiscard]] auto createShaderModule(const std::vector<char> &code) const
      -> vk::raii::ShaderModule;

  VulkanContext &m_context;
  Scene &m_scene;
  QueueHandle &m_queueHandle;

  uint32_t m_framesInFlight{1};
  uint32_t m_currentFrame{0};

  Swapchain m_swapchain;

  std::optional<Image> m_depthImage;
  std::optional<Image> m_colorImage;

  std::vector<Buffer> m_cameraUniformBuffers;
  std::vector<void *> m_cameraUniformBuffersMapped;

  vk::raii::ImageView m_depthImageView = nullptr;
  vk::raii::ImageView m_colorImageView = nullptr;

  vk::raii::DescriptorSetLayout m_globalDescriptorSetLayout = nullptr;
  vk::raii::DescriptorSetLayout m_materialDescriptorSetLayout = nullptr;
  vk::raii::DescriptorSetLayout m_objectDescriptorSetLayout = nullptr;

  vk::raii::DescriptorPool m_descriptorPool = nullptr;

  // Descriptor sets for the global uniform buffer (camera) for each frame in flight, as there are
  // multiple camera uniform buffers, one for each frame in flight.
  std::vector<vk::raii::DescriptorSet> m_globalDescriptorSets;

  // Descriptor sets for the material (texture) for each object in the scene, as there are multiple
  // objects, each with its own texture.
  std::vector<vk::raii::DescriptorSet> m_materialDescriptorSets;

  vk::raii::PipelineLayout m_pipelineLayout = nullptr;
  vk::raii::Pipeline m_graphicsPipeline = nullptr;

  std::vector<vk::raii::CommandBuffer> m_commandBuffers;
  std::vector<vk::raii::Semaphore> m_imageAvailableSemaphores;
  std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;
  std::vector<vk::raii::Fence> m_inFlightFences;
};
} // namespace vksim