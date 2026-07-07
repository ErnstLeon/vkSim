#pragma once

#include <cstdint>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vector"

#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/window/Window.hpp"

namespace vksim
{

/**
 * @brief Structure to hold information for creating a Vulkan swapchain.
 */
struct SwapchainCreateInfo
{
  vk::Format format;
  vk::ColorSpaceKHR colorSpace;
  uint32_t imageCount{3};

  vk::ImageUsageFlags imageUsage{vk::ImageUsageFlagBits::eColorAttachment};
  vk::SharingMode imageSharingMode{vk::SharingMode::eExclusive};
  vk::SurfaceTransformFlagBitsKHR preTransform{vk::SurfaceTransformFlagBitsKHR::eIdentity};
  vk::CompositeAlphaFlagBitsKHR compositeAlpha{vk::CompositeAlphaFlagBitsKHR::eOpaque};
};

/**
 * @brief Swapchain class encapsulates the Vulkan swapchain and manages
 *        its associated resources, including images and image views.
 */
class Swapchain
{
public:
  /**
   * @brief Initializes a Swapchain with the specified Vulkan context.
   * @param context Reference to the VulkanContext object.
   */
  Swapchain(VulkanContext &context);

  Swapchain(const Swapchain &) = delete;
  auto operator=(const Swapchain &) noexcept -> Swapchain & = delete;

  Swapchain(Swapchain &&) noexcept = default;
  auto operator=(Swapchain &&) noexcept -> Swapchain & = delete;

  /**
   * @brief Creates the Vulkan swapchain with the specified create info. This method initializes the
   * swapchain, retrieves the swapchain images, and creates image views for each image.
   * @param createInfo Structure containing information for creating the
   * swapchain.
   */
  auto create(const SwapchainCreateInfo &createInfo) -> void;

  /**
   * @brief Recreates the swapchain, typically called when the window is
   * resized or when the swapchain becomes incompatible with the surface.
   *        This function cleans up the existing swapchain and its
   * associated resources, and then creates a new swapchain with updated
   * parameters.
   */
  auto recreate() -> void;

  /**
   * @brief Transitions the layout of the swapchain images to a new layout.
   * @param imageIndex The index of the swapchain image to transition.
   * @param old_layout The current layout of the image.
   * @param new_layout The desired layout for the image.
   * @param src_access_mask The source access mask for the transition.
   * @param dst_access_mask The destination access mask for the transition.
   * @param src_stage_mask The source pipeline stage mask for the transition.
   * @param dst_stage_mask The destination pipeline stage mask for the transition.
   * @param aspect The aspect mask for the image.
   * @param commandBuffer The command buffer used for recording the
   * transition commands.
   */
  auto transitionLayout(uint32_t imageIndex, vk::ImageLayout old_layout, vk::ImageLayout new_layout,
                        vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
                        vk::PipelineStageFlags2 src_stage_mask,
                        vk::PipelineStageFlags2 dst_stage_mask, vk::ImageAspectFlagBits aspect,
                        vk::raii::CommandBuffer &commandBuffer) -> void;

  [[nodiscard]] auto get() const -> const vk::raii::SwapchainKHR &;
  [[nodiscard]] auto getImages() const -> const std::vector<vk::Image> &;
  [[nodiscard]] auto getSurfaceFormat() const -> const vk::SurfaceFormatKHR &;
  [[nodiscard]] auto getExtent() const -> const vk::Extent2D &;
  [[nodiscard]] auto getImageViews() const -> const std::vector<vk::raii::ImageView> &;

private:
  /**
   * @brief Chooses the swapchain extent based on the surface
   * capabilities and the GLFW window size.
   * @param surfaceCapabilities The surface capabilities of the Vulkan
   * physical device.
   * @param window Reference to the vksim::Window object.
   * @return The chosen swapchain extent as a vk::Extent2D.
   */
  static auto chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities,
                               const vksim::Window &window) -> vk::Extent2D;

  /**
   * @brief Chooses the minimum number of images for the swapchain based on
   * the surface capabilities.
   * @param surfaceCapabilities The surface capabilities of the Vulkan
   * physical device.
   * @param imageCount The desired number of images for the swapchain.
   * @return The chosen minimum number of images for the swapchain.
   */
  static auto chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities,
                                      uint32_t imageCount) -> uint32_t;

  /**
   * @brief Chooses the swapchain present mode based on the available
   * present modes.
   * @param availablePresentModes A vector of available present modes.
   * @return The chosen swapchain present mode as a vk::PresentModeKHR.
   */
  static auto chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
      -> vk::PresentModeKHR;

  /**
   * @brief Chooses the swapchain surface format based on the available
   * surface formats.
   * @param availableFormats A vector of available surface formats.
   * @return The chosen swapchain surface format as a vk::SurfaceFormatKHR.
   */
  static auto chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats,
                                      vk::Format format, vk::ColorSpaceKHR colorSpace)
      -> vk::SurfaceFormatKHR;

  vk::raii::SwapchainKHR m_swapChain = nullptr;
  std::vector<vk::Image> m_swapChainImages;
  std::vector<vk::raii::ImageView> m_swapChainImageViews;
  vk::SurfaceFormatKHR m_swapChainSurfaceFormat{};
  vk::Extent2D m_swapChainExtent{};

  VulkanContext &m_context;
  SwapchainCreateInfo m_createInfo{};
};
} // namespace vksim