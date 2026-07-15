#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <expected>
#include <vulkan/vulkan_raii.hpp>

#include "vksim/render/buffers/Buffer.hpp"
#include "vksim/render/buffers/Image.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim
{

Image::Image(vksim::Device &device) : m_device(device) {};

auto Image::create(const ImageCreateInfo &createInfo) -> void
{
  vk::ImageCreateInfo imageInfo{
      .imageType = vk::ImageType::e2D,
      .format = createInfo.format,
      .extent = {.width = createInfo.width, .height = createInfo.height, .depth = 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = createInfo.numSamples,
      .tiling = createInfo.tiling,
      .usage = createInfo.usage,
      .sharingMode = vk::SharingMode::eExclusive,
      .initialLayout = vk::ImageLayout::eUndefined};

  m_image = std::move(vk::raii::Image(m_device.logical(), imageInfo));
  vk::MemoryRequirements memRequirements = m_image.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex =
          m_device.findMemoryType(memRequirements.memoryTypeBits, createInfo.properties)
              .value_or(0)};
  m_imageMemory = vk::raii::DeviceMemory(m_device.logical(), allocInfo);
  m_image.bindMemory(*m_imageMemory, 0);
}

auto Image::getVkImage() const -> const vk::raii::Image & { return m_image; }

auto Image::getVkImageMemory() const -> const vk::raii::DeviceMemory & { return m_imageMemory; }

auto Image::getVkImageView(const ImageViewCreateInfo &createInfo) const -> vk::raii::ImageView
{
  vk::ImageViewCreateInfo viewInfo{.image = *m_image,
                                   .viewType = vk::ImageViewType::e2D,
                                   .format = createInfo.format,
                                   .subresourceRange = {.aspectMask = createInfo.aspectFlags,
                                                        .baseMipLevel = 0,
                                                        .levelCount = 1,
                                                        .baseArrayLayer = 0,
                                                        .layerCount = 1}};
  return {m_device.logical(), viewInfo};
}

auto Image::copyFromBuffer(Buffer &buffer, uint32_t width, uint32_t height,
                           vk::raii::CommandBuffer &commandBuffer) const -> void
{
  vk::BufferImageCopy region{.bufferOffset = 0,
                             .bufferRowLength = 0,
                             .bufferImageHeight = 0,
                             .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                                  .mipLevel = 0,
                                                  .baseArrayLayer = 0,
                                                  .layerCount = 1},
                             .imageOffset = {.x = 0, .y = 0, .z = 0},
                             .imageExtent = {.width = width, .height = height, .depth = 1}};
  commandBuffer.copyBufferToImage(*buffer.getVkBuffer(), *m_image,
                                  vk::ImageLayout::eTransferDstOptimal, region);
}

auto Image::transitionLayout(vk::ImageLayout old_layout, vk::ImageLayout new_layout,
                             vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
                             vk::PipelineStageFlags2 src_stage_mask,
                             vk::PipelineStageFlags2 dst_stage_mask, vk::ImageAspectFlagBits aspect,
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
                                     .image = *m_image,
                                     .subresourceRange = {.aspectMask = aspect,
                                                          .baseMipLevel = 0,
                                                          .levelCount = 1,
                                                          .baseArrayLayer = 0,
                                                          .layerCount = 1}};
  vk::DependencyInfo dependency_info = {
      .dependencyFlags = {}, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier};
  commandBuffer.pipelineBarrier2(dependency_info);
}

} // namespace vksim