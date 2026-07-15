#pragma once

#include <expected>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/render/context/VulkanContext.hpp"
#include "vksim/render/device/Device.hpp"

namespace vksim
{

// Forward declaration of the Buffer class to avoid circular dependencies
class Buffer;

/**
 * @brief Structure to hold information for creating a Vulkan image.
 */
struct ImageCreateInfo
{
  uint32_t width{};
  uint32_t height{};
  vk::SampleCountFlagBits numSamples{vk::SampleCountFlagBits::e1};
  vk::Format format{};
  vk::ImageTiling tiling{};
  vk::ImageUsageFlags usage;
  vk::MemoryPropertyFlags properties;
};

struct ImageViewCreateInfo
{
  vk::Format format{};
  vk::ImageAspectFlags aspectFlags;
};

/** @brief Image class encapsulates a Vulkan image and manages its
 *        associated resources. It provides methods for creating, copying, and accessing the image
 * and its memory. The Image class is responsible for allocating and freeing the image memory, as
 * well as providing access to the underlying Vulkan image and device memory objects. It must be
 * initialized with a vksim::Device reference to ensure proper resource management and lifetime
 * control.
 */
class Image
{
public:
  Image(const Image &) = delete;
  Image(Image &&) noexcept = default;

  auto operator=(const Image &) -> Image & = delete;
  auto operator=(Image &&) -> Image & = delete;

  /** @brief Initializes an Image with the specified create info.
   * @param device The Vulkan device to use for image creation.
   */
  Image(vksim::Device &device);

  /** @brief Creates a Vulkan image with the specified create info. This method allocates the
   * image and its associated memory, and binds them together. It must be called after the Image
   * object is constructed and before any operations are performed on the image. The createInfo
   * parameter specifies the width, height, number of samples, format, tiling, usage, and memory
   * properties of the image to be created.
   * @param createInfo Structure containing information for creating the
   * image.
   */
  auto create(const ImageCreateInfo &createInfo) -> void;

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

private:
  vk::raii::Image m_image = nullptr;
  vk::raii::DeviceMemory m_imageMemory = nullptr;

  uint32_t m_width{};
  uint32_t m_height{};

  vksim::Device &m_device;
};

} // namespace vksim