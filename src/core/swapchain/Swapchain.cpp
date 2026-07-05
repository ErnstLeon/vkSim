#include "spdlog/spdlog.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/swapchain/Swapchain.hpp"

namespace vksim
{

Swapchain::Swapchain(VulkanContext *context, const SwapchainCreateInfo &createInfo)
    : m_context(context), m_createInfo(createInfo)
{
  GLFWwindow *window = createInfo.window;

  vk::SurfaceCapabilitiesKHR surfaceCapabilities =
      context->getPhysicalDevice().getSurfaceCapabilitiesKHR(context->getSurface());
  m_swapChainExtent = chooseSwapExtent(surfaceCapabilities, createInfo.window);

  uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities, createInfo.imageCount);

  std::vector<vk::SurfaceFormatKHR> availableFormats =
      context->getPhysicalDevice().getSurfaceFormatsKHR(context->getSurface());
  m_swapChainSurfaceFormat =
      chooseSwapSurfaceFormat(availableFormats, createInfo.format, createInfo.colorSpace);

  std::vector<vk::PresentModeKHR> availablePresentModes =
      context->getPhysicalDevice().getSurfacePresentModesKHR(context->getSurface());

  vk::SwapchainCreateInfoKHR swapChainCreateInfo{
      .surface = context->getSurface(),
      .minImageCount = minImageCount,
      .imageFormat = m_swapChainSurfaceFormat.format,
      .imageColorSpace = m_swapChainSurfaceFormat.colorSpace,
      .imageExtent = m_swapChainExtent,
      .imageArrayLayers = 1,
      .imageUsage = createInfo.imageUsage,
      .imageSharingMode = createInfo.imageSharingMode,
      .preTransform = createInfo.preTransform,
      .compositeAlpha = createInfo.compositeAlpha,
      .presentMode = chooseSwapPresentMode(availablePresentModes),
      .clipped = 1U};

  m_swapChain = vk::raii::SwapchainKHR(context->getDevice(), swapChainCreateInfo);
  m_swapChainImages = m_swapChain.getImages();

  // Create image views for each swapchain image
  m_swapChainImageViews.reserve(m_swapChainImages.size());
  for (const auto &image : m_swapChainImages)
  {
    vk::ImageViewCreateInfo viewCreateInfo{
        .image = image,
        .viewType = vk::ImageViewType::e2D,
        .format = m_swapChainSurfaceFormat.format,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};
    m_swapChainImageViews.emplace_back(m_context->getDevice(), viewCreateInfo);
  }
}

auto Swapchain::recreate() -> void
{
  // Clean up existing swapchain resources
  m_swapChainImageViews.clear();
  m_swapChainImages.clear();
  m_swapChain = nullptr;
  // Recreate the swapchain with the same create info
  *this = Swapchain(m_context, m_createInfo);
}

auto Swapchain::transitionLayout(uint32_t imageIndex, vk::ImageLayout old_layout,
                                 vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask,
                                 vk::AccessFlags2 dst_access_mask,
                                 vk::PipelineStageFlags2 src_stage_mask,
                                 vk::PipelineStageFlags2 dst_stage_mask,
                                 vk::ImageAspectFlagBits aspect,
                                 vk::raii::CommandBuffer &commandBuffer) -> void
{
  vk::ImageMemoryBarrier2 barrier = {.srcStageMask = src_stage_mask,
                                     .srcAccessMask = src_access_mask,
                                     .dstStageMask = dst_stage_mask,
                                     .dstAccessMask = dst_access_mask,
                                     .oldLayout = old_layout,
                                     .newLayout = new_layout,
                                     .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                     .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                     .image = m_swapChainImages[imageIndex],
                                     .subresourceRange = {.aspectMask = aspect,
                                                          .baseMipLevel = 0,
                                                          .levelCount = 1,
                                                          .baseArrayLayer = 0,
                                                          .layerCount = 1}};
  vk::DependencyInfo dependencyInfo{
      .dependencyFlags = {}, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier};
  commandBuffer.pipelineBarrier2(dependencyInfo);
}

[[nodiscard]] auto Swapchain::get() const -> const vk::raii::SwapchainKHR & { return m_swapChain; }

[[nodiscard]] auto Swapchain::getImages() const -> const std::vector<vk::Image> &
{
  return m_swapChainImages;
}

[[nodiscard]] auto Swapchain::getSurfaceFormat() const -> const vk::SurfaceFormatKHR &
{
  return m_swapChainSurfaceFormat;
}

[[nodiscard]] auto Swapchain::getExtent() const -> const vk::Extent2D &
{
  return m_swapChainExtent;
}

[[nodiscard]] auto Swapchain::getImageViews() const -> const std::vector<vk::raii::ImageView> &
{
  return m_swapChainImageViews;
}

auto Swapchain::chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities,
                                 GLFWwindow *window) -> vk::Extent2D
{
  if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
  {
    return surfaceCapabilities.currentExtent;
  }
  int width;
  int height;
  glfwGetFramebufferSize(window, &width, &height);

  return {.width = std::clamp<uint32_t>(width, surfaceCapabilities.minImageExtent.width,
                                        surfaceCapabilities.maxImageExtent.width),
          .height = std::clamp<uint32_t>(height, surfaceCapabilities.minImageExtent.height,
                                         surfaceCapabilities.maxImageExtent.height)};
}

auto Swapchain::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats,
                                        vk::Format format, vk::ColorSpaceKHR colorSpace)
    -> vk::SurfaceFormatKHR
{
  const auto formatIt =
      std::ranges::find_if(availableFormats, [format, colorSpace](const auto &sfm) -> auto
                           { return sfm.format == format && sfm.colorSpace == colorSpace; });
  return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}

auto Swapchain::chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities,
                                        uint32_t imageCount) -> uint32_t
{
  auto minImageCount = std::max(imageCount, surfaceCapabilities.minImageCount);
  if ((0 < surfaceCapabilities.maxImageCount) &&
      (surfaceCapabilities.maxImageCount < minImageCount))
  {
    minImageCount = surfaceCapabilities.maxImageCount;
  }
  return minImageCount;
}

auto Swapchain::chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
    -> vk::PresentModeKHR
{
  assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) -> auto
                             { return presentMode == vk::PresentModeKHR::eFifo; }));
  return std::ranges::any_of(availablePresentModes, [](const vk::PresentModeKHR value) -> bool
                             { return vk::PresentModeKHR::eMailbox == value; })
             ? vk::PresentModeKHR::eMailbox
             : vk::PresentModeKHR::eFifo;
}

} // namespace vksim