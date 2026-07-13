#pragma once

#include <array>
#include <optional>
#include <stdexcept>
#include <vector>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define IMGUI_DEFINE_MATH_OPERATORS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/buffers/Buffer.hpp"
#include "vksim/core/buffers/Image.hpp"
#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/device/Device.hpp"
#include "vksim/core/render/Swapchain.hpp"
#include "vksim/core/scene/Scene.hpp"

namespace vksim::ImGui
{
/** @brief ImGuiRenderer class for managing ImGui integration with Vulkan.
 *        This class handles the initialization, rendering, and cleanup of ImGui resources,
 *        including descriptor pools, command buffers, and rendering contexts. It provides
 *        methods for updating ImGui state, handling input events, and recording command buffers
 *        for rendering ImGui elements in a Vulkan application.
 */
class ImGuiRenderer
{
public:
  /** @brief Constructor for ImGuiRenderer.
   *  @param context Reference to the Vulkan context for resource management.
   *  @param swapchain Reference to the swapchain for rendering ImGui.
   *  @param scene Reference to the scene for ImGui rendering.
   */
  ImGuiRenderer(VulkanContext &context, Swapchain &swapchain, Scene &scene);
  ~ImGuiRenderer();

  // Core functionality methods for ImGui integration
  void init();    // Initialize ImGui context and configure display
  void destroy(); // Clean up ImGui resources and context

  /** @brief Update ImGui state and handle input events. */
  auto update() -> void; // Update ImGui state and handle input events

  /** @brief Handle swapchain recreation events, refreshing ImGui backend state.
   *  This method should be called when the swapchain is recreated, such as during window
   *  resizing or format changes. It updates the ImGui backend to ensure proper rendering
   *  with the new swapchain configuration.
   */
  auto recreateWithSwapchain() -> void; // Refresh ImGui backend state after swapchain resize

  /** @brief Record command buffer for ImGui rendering.
   *  @param commandBuffer Reference to the Vulkan command buffer for recording ImGui commands.
   *  @param imageIndex Index of the swapchain image to render ImGui onto.
   */
  auto recordCommandBuffer(vk::raii::CommandBuffer &commandBuffer, uint32_t imageIndex)
      -> void; // Record command buffer for ImGui rendering

private:
  void createDescriptorPool(); // Create descriptor pool for ImGui resources

  void initImGui(); // Initialize ImGui context and configure display

  VulkanContext &m_context; // Reference to the Vulkan context for resource management
  Swapchain &m_swapchain;   // Reference to the swapchain for rendering ImGui
  Scene &m_scene;           // Reference to the scene for ImGui rendering

  int m_framebufferWidth{0};                  // Width of the framebuffer for ImGui rendering
  int m_framebufferHeight{0};                 // Height of the framebuffer for ImGui rendering
  std::optional<uint32_t> m_selectedObjectId; // Optional ID of the currently selected scene object
                                              // for ImGui interaction

  vk::raii::DescriptorPool m_descriptorPool{nullptr}; // Descriptor pool for ImGui resources
};

} // namespace vksim::ImGui