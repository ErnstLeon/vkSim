#pragma once
#include <deque>
#include <unordered_map>
#include <unordered_set>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define IMGUI_DEFINE_MATH_OPERATORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/buffers/Image.hpp"
#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/render/Swapchain.hpp"
#include "vksim/core/scene/Scene.hpp"
#include "vksim/imgui/ImGuiUtil.hpp"

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
   * @param framesInFlight The number of frames that can be processed concurrently.
   * @param enableImGui Flag to enable or disable ImGui rendering.
   */
  Renderer(VulkanContext &context, Scene &scene, uint32_t framesInFlight = 1,
           bool enableImGui = true);

  /** @brief Renders a frame. */
  auto drawFrame() -> void;
  [[nodiscard]] auto getExtent() const -> vk::Extent2D { return m_swapchain.getExtent(); }

private:
  /** @brief Creates the descriptor pool. */
  auto createDescriptorPool() -> void;

  /** @brief Creates the descriptor set layouts. */
  auto createDescriptorSetLayouts() -> void;

  /** @brief Creates the descriptor sets. */
  auto createDescriptorSets() -> void;

  /** @brief Creates the graphics pipeline. */
  auto createCommandBuffers() -> void;

  /** @brief Records the command buffer for the specified image index and frame index. */
  auto recordCommandBuffer(uint32_t imageIndex, uint32_t frameIndex,
                           vk::raii::CommandBuffer &commandBuffer) -> void;

  /** @brief Creates the swapchain. */
  auto createSwapchain() -> void;

  /** @brief Recreates the swapchain and associated resources. */
  auto recreateSwapchain() -> void;

  /** @brief Creates the synchronization objects. */
  auto createSyncObjects() -> void;

  /** @brief Creates the resources for the scene. Those are host-visible and host-coherent buffers
   * for each frame in flight. This allows for fast CPU-side updates.
   */
  auto createSceneResources() -> void;

  /** @brief Creates the resources for the camera. Those are host-visible and host-coherent buffers
   * for each frame in flight. This allows for fast CPU-side updates.
   */
  auto createCameraResources() -> void;

  /** @brief Creates the resources for the lights. Those are host-visible and host-coherent buffers
   * for each frame in flight. This allows for fast CPU-side updates.
   */
  auto createLightResources() -> void;

  /** @brief Creates the depth resources. Those are images and image views used for depth testing.
   */
  auto createDepthResources() -> void;

  /** @brief Creates the color resources. Those are images and image views used for color
   * attachments, necessary for MSAA.
   */
  auto createColorResources() -> void;

  /** @brief Creates the graphics pipeline. */
  auto createGraphicsPipeline() -> void;

  /** @brief Extracts the unique materials and textures from the scene. This helps in reducing
   * redundant resource creation and improves performance. Materials and textures will be boundes as
   * descriptor arrays to the graphics pipeline, and the indices of the materials and textures in
   * these arrays will be used as IDs in the ObjectDescriptor struct to associate each scene object
   * with its corresponding material and texture.
   */
  auto extractUniqueMaterialsAndTextures() -> void;

  /** @brief Updates the scene data for the current frame. This includes updating the uniform
   * buffers for the scene information, camera, and lights.
   */
  auto updateSceneDataForCurrentFrame() -> void;

  /** @brief Prepares the ImGui renderer. */
  auto prepareImGui() -> void;

  /** @brief Creates a shader module from the given SPIR-V code.
   * @param code The SPIR-V code for the shader.
   * @return The created shader module.
   */
  [[nodiscard]] auto createShaderModule(const std::vector<char> &code) const
      -> vk::raii::ShaderModule;

  // Reference to the Vulkan context and scene.
  VulkanContext &m_context;
  Scene &m_scene;

  // Owning the swapchain, as it is tightly coupled with the renderer and its resources.
  Swapchain m_swapchain;

  uint32_t m_framesInFlight{1};
  uint32_t m_currentFrame{0};

  std::optional<vksim::ImGui::ImGuiRenderer> m_imguiRenderer{std::nullopt};

  // Depth and color resources for the swapchain images.
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
   * HostVisible and HostCoherent to allow for CPU-side updates. There will be one buffer per frame
   * in flight for each type of light (directional, point, and spot). Each buffer will contain an
   * array of light properties for all lights of that type in the scene. The mapped memory pointers
   * will be used to update the buffers with the latest light data each frame.
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

  // Command buffers for each frame in flight. Each command buffer records the rendering commands
  // for a single frame, including binding the graphics pipeline, descriptor sets, and drawing the
  // scene objects. The command buffers are submitted to the graphics queue for execution.
  std::vector<vk::raii::CommandBuffer> m_commandBuffers;
  std::vector<vk::raii::Semaphore> m_imageAvailableSemaphores;
  std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;
  std::vector<vk::raii::Fence> m_inFlightFences;
};
} // namespace vksim