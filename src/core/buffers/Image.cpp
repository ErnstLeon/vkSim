#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <expected>
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/buffers/Buffer.hpp"
#include "vksim/core/buffers/Image.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim
{

Image::Image(VulkanContext &context) : m_context(context) {};

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

  m_height = createInfo.height;
  m_width = createInfo.width;

  m_image = std::move(vk::raii::Image(m_context.getDevice().logical(), imageInfo));
  vk::MemoryRequirements memRequirements = m_image.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = m_context.getDevice()
                             .findMemoryType(memRequirements.memoryTypeBits, createInfo.properties)
                             .value_or(0)};
  m_imageMemory = vk::raii::DeviceMemory(m_context.getDevice().logical(), allocInfo);
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
  return {m_context.getDevice().logical(), viewInfo};
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

auto Image::copyFromHost(const void *data, uint32_t size) -> void
{
  // Get the default transfer queue and its associated command pool from the Vulkan context
  const auto &defaultQueue = m_context.getDefaultTransferQueue();
  const auto &commandPool = m_context.getCommandPool(defaultQueue.familyIndex);

  // Allocate a command buffer from the command pool
  vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool.get(),
                                          .level = vk::CommandBufferLevel::ePrimary,
                                          .commandBufferCount = 1};
  auto commandBuffers = vk::raii::CommandBuffers(m_context.getDevice().logical(), allocInfo);

  // Begin recording commands into the command buffer
  commandBuffers.front().begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  // Create a staging buffer for host-visible memory
  Buffer stagingBuffer(m_context);
  stagingBuffer.create(BufferCreateInfo{.size = size,
                                        .usage = vk::BufferUsageFlagBits::eTransferSrc,
                                        .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                                      vk::MemoryPropertyFlagBits::eHostCoherent});

  // Map the staging buffer memory, copy data from host to the mapped memory, and unmap it
  auto *mappedMemory = stagingBuffer.getVkBufferMemory().mapMemory(0, size);
  std::memcpy(mappedMemory, data, static_cast<size_t>(size));
  stagingBuffer.getVkBufferMemory().unmapMemory();

  // Transition the image layout to be optimal for transfer destination
  transitionLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, {},
                   vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eTopOfPipe,
                   vk::PipelineStageFlagBits2::eTransfer, vk::ImageAspectFlagBits::eColor,
                   commandBuffers[0]);

  // Copy data from the staging buffer to the Vulkan image using the command buffer
  copyFromBuffer(stagingBuffer, m_width, m_height, commandBuffers[0]);

  // Transition the image layout to be optimal for shader read access
  transitionLayout(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                   vk::AccessFlagBits2::eTransferWrite, vk::AccessFlagBits2::eShaderRead,
                   vk::PipelineStageFlagBits2::eTransfer,
                   vk::PipelineStageFlagBits2::eFragmentShader, vk::ImageAspectFlagBits::eColor,
                   commandBuffers[0]);

  // End recording commands into the command buffer
  commandBuffers[0].end();

  // Submit the command buffer to the default transfer queue for execution and wait for completion
  vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandBuffers.front()};
  defaultQueue.vkQueue.submit(submitInfo, nullptr);
  defaultQueue.vkQueue.waitIdle();
}
} // namespace vksim