#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/buffers/Buffer.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim
{

Buffer::Buffer(VulkanContext *context, const BufferCreateInfo &createInfo) : m_context(context)
{
  vk::BufferCreateInfo bufferInfo{.size = createInfo.size,
                                  .usage = createInfo.usage,
                                  .sharingMode = vk::SharingMode::eExclusive};

  m_buffer = vk::raii::Buffer(m_context->getDevice(), bufferInfo);
  vk::MemoryRequirements memRequirements = m_buffer.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex =
          findMemoryType(m_context, memRequirements.memoryTypeBits, createInfo.properties)};

  m_bufferMemory = vk::raii::DeviceMemory(m_context->getDevice(), allocInfo);
  m_buffer.bindMemory(*m_bufferMemory, 0);
}

auto Buffer::findMemoryType(VulkanContext *context, uint32_t typeFilter,
                            vk::MemoryPropertyFlags properties) -> uint32_t
{
  vk::PhysicalDeviceMemoryProperties memProperties =
      context->getPhysicalDevice().getMemoryProperties();

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
  {
    if ((typeFilter & (1U << i)) != 0U &&
        (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
    {
      return i;
    }
  }

  spdlog::error("Failed to find suitable memory type!");
  std::abort();
}

auto Buffer::copyFromBuffer(Buffer &buffer, uint32_t size,
                            vk::raii::CommandBuffer &commandBuffer) const -> void
{
  vk::BufferCopy copyRegion{.srcOffset = 0, .dstOffset = 0, .size = size};
  commandBuffer.copyBuffer(*buffer.getVkBuffer(), *m_buffer, copyRegion);
}

auto Buffer::getVkBuffer() const -> const vk::raii::Buffer & { return m_buffer; }

auto Buffer::getVkBufferMemory() const -> const vk::raii::DeviceMemory & { return m_bufferMemory; }

} // namespace vksim
