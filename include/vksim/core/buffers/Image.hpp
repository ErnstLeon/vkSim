#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <expected>
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/context/VulkanContext.hpp"

namespace vksim
{

// Forward declaration of the Buffer class to avoid circular dependencies
class Buffer;

/**
 * @brief Structure to hold information for creating a Vulkan image.
 */
struct ImageCreateInfo
{
  uint32_t width;
  uint32_t height;
  vk::SampleCountFlagBits numSamples;
  vk::Format format;
  vk::ImageTiling tiling;
  vk::ImageUsageFlags usage;
  vk::MemoryPropertyFlags properties;
};

struct ImageViewCreateInfo
{
  vk::Format format;
  vk::ImageAspectFlags aspectFlags;
};

class Image
{
public:
  Image() = default;
  Image(const Image &) = delete;
  Image(Image &&) noexcept = default;

  auto operator=(const Image &) -> Image & = delete;
  auto operator=(Image &&) -> Image & = default;

  /** @brief Constructs an Image with the specified create info.
   * @param device The Vulkan device to use for image creation.
   * @param createInfo Structure containing information for creating the
   * image.
   */
  Image(VulkanContext *context, const ImageCreateInfo &createInfo);

  /** @brief Returns the underlying Vulkan image.
   * @return Reference to the Vulkan image.
   */
  [[nodiscard]] auto getVkImage() const -> const vk::raii::Image &;

  /** @brief Returns the memory allocated for the image.
   * @return Reference to the device memory.
   */
  [[nodiscard]] auto getVkImageMemory() const -> const vk::raii::DeviceMemory &;

  /** @brief Returns the image view for the image.
   * @param createInfo Structure containing information for creating the
   * image view.
   * @return Reference to the image view.
   */
  [[nodiscard]] auto getVkImageView(const ImageViewCreateInfo &createInfo) const
      -> vk::raii::ImageView;

  /** @brief Copies data from a buffer to the image.
   * @param buffer The source buffer to copy data from.
   * @param width The width of the image.
   * @param height The height of the image.
   * @param commandBuffer The command buffer to record the copy commands.
   */
  auto copyFromBuffer(Buffer &buffer, uint32_t width, uint32_t height,
                      vk::raii::CommandBuffer &commandBuffer) const -> void;

  /** @brief Transitions the image layout to a new layout.
   * @param old_layout The current layout of the image.
   * @param new_layout The desired layout to transition to.
   * @param src_access_mask The source access mask for the transition.
   * @param dst_access_mask The destination access mask for the transition.
   * @param src_stage_mask The source pipeline stage for the transition.
   * @param dst_stage_mask The destination pipeline stage for the transition.
   * @param aspect The aspect of the image to transition.
   * @param commandBuffer The command buffer to record the transition commands.
   */
  auto transitionLayout(vk::ImageLayout old_layout, vk::ImageLayout new_layout,
                        vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
                        vk::PipelineStageFlags2 src_stage_mask,
                        vk::PipelineStageFlags2 dst_stage_mask, vk::ImageAspectFlagBits aspect,
                        vk::raii::CommandBuffer &commandBuffer) -> void;

  /** @brief Finds a suitable depth format for the image.
   * @param context The Vulkan context.
   * @return The found depth format.
   */
  static auto findDepthFormat(VulkanContext *context) -> std::expected<vk::Format, std::string>;

private:
  /** @brief Finds a suitable memory type based on the filter and
   * properties.
   * @param context The Vulkan context.
   * @param typeFilter The type of memory to filter.
   * @param properties The required memory properties.
   * @return The index of the found memory type.
   */
  static auto findMemoryType(VulkanContext *context, uint32_t typeFilter,
                             vk::MemoryPropertyFlags properties) -> uint32_t;

  /** @brief Finds a supported format from a list of candidates.
   * @param context The Vulkan context.
   * @param candidates The list of candidate formats to check.
   * @param tiling The desired image tiling.
   * @param features The required format features.
   * @return The first supported format found in the candidates.
   */
  static auto findSupportedFormat(VulkanContext *context, const std::vector<vk::Format> &candidates,
                                  vk::ImageTiling tiling, vk::FormatFeatureFlags features)
      -> std::expected<vk::Format, std::string>;

  vk::raii::Image m_image = nullptr;
  vk::raii::DeviceMemory m_imageMemory = nullptr;

  uint32_t m_width{};
  uint32_t m_height{};

  VulkanContext *m_context = nullptr;
};

} // namespace vksim