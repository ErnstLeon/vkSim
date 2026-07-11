#pragma once

#include "vksim/core/buffers/Image.hpp"
#include <deque>
#include <unordered_map>
#include <unordered_set>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/render/Swapchain.hpp"
#include "vksim/core/scene/Scene.hpp"

namespace vksim
{

/** @brief Structure representing the push constant data for the vertex shader, including the model
 * matrix and IDs for the texture and material. */
struct ObjectDescriptor
{
  glm::mat4 modelMatrix;
  glm::mat4 normalMatrix;
  uint32_t textureId;
  uint32_t materialId;
};

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
  auto createSceneResources() -> void;
  auto createCameraResources() -> void;
  auto createLightResources() -> void;
  auto createDepthResources() -> void;
  auto createColorResources() -> void;
  auto createGraphicsPipeline() -> void;
  auto extractUniqueMaterialsAndTextures() -> void;
  auto updateSceneDataForCurrentFrame() -> void;

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

  vk::raii::ImageView m_depthImageView = nullptr;
  vk::raii::ImageView m_colorImageView = nullptr;

  // Buffer containing the scene information, including the number of lights in the scene. This
  // buffer is updated every frame to reflect the current state of the scene and is used in the
  // shaders to determine how to render the scene.
  std::vector<Buffer> m_SceneInfoUniformBuffer;
  std::vector<void *> m_SceneInfoUniformBufferMapped;

  // Buffers and mapped memory for the camera uniform buffer for each frame in flight. The camera's
  // uniform buffer will be updated each frame with the camera's view and projection matrices. The
  // buffer is HostVisible and HostCoherent to allow for CPU-side updates.
  std::vector<Buffer> m_cameraUniformBuffers;
  std::vector<void *> m_cameraUniformBuffersMapped;

  /** @brief Buffers and mapped memory for the light uniform buffers for each frame in flight. The
   * light's uniform buffers will be updated each frame with the light's properties. The buffers are
   * HostVisible and HostCoherent to allow for CPU-side updates.
   */
  std::vector<Buffer> m_directionalLightBuffers;
  std::vector<void *> m_directionalLightBuffersMapped;
  std::vector<Buffer> m_pointLightBuffers;
  std::vector<void *> m_pointLightBuffersMapped;
  std::vector<Buffer> m_spotLightBuffers;
  std::vector<void *> m_spotLightBuffersMapped;

  /** @brief Structure to hold the unique materials and textures in the scene. Each unique material
   * and texture will have a corresponding descriptor set that will be bound to the graphics
   * pipeline during rendering. The indices of the materials and textures in these vectors are used
   * as IDs in the ObjectDescriptor struct to associate each scene object with its corresponding
   * material and texture.
   */
  std::vector<std::string> m_uniqueMaterials;
  std::vector<std::string> m_uniqueTextures;

  vk::raii::DescriptorPool m_descriptorPool = nullptr;

  /** @brief Descriptor set layout for the uniform buffers of camera. Binding 0 -> Camera UBO.
   */
  vk::raii::DescriptorSetLayout m_frameDescriptorSetLayout = nullptr;

  /** @brief Descriptor set layout for the combined image sampler of texture and uniform buffer of
   * material. Binding 0 -> Combined Image Sampler (Texture), Binding 1 -> Material UBO. Both are
   * descriptor arrays for each unique texture and material in the scene.
   */
  vk::raii::DescriptorSetLayout m_materialDescriptorSetLayout = nullptr;

  // Descriptor sets for the camera for each frame in flight.
  std::vector<vk::raii::DescriptorSet> m_frameDescriptorSets;

  // Descriptor sets for all unique combined image sampler (texture) and uniform buffer (material)
  // in the scene.
  std::vector<vk::raii::DescriptorSet> m_materialDescriptorSets;

  // Vectors to map scene objects to their corresponding texture and material descriptor sets.
  // These mappings are used to bind the correct texture and material properties to each object
  // during rendering and are submitted as push constants to the vertex shader.
  std::vector<ObjectDescriptor> m_objectDescriptors;

  vk::raii::PipelineLayout m_pipelineLayout = nullptr;
  vk::raii::Pipeline m_graphicsPipeline = nullptr;

  std::vector<vk::raii::CommandBuffer> m_commandBuffers;
  std::vector<vk::raii::Semaphore> m_imageAvailableSemaphores;
  std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;
  std::vector<vk::raii::Fence> m_inFlightFences;
};
} // namespace vksim