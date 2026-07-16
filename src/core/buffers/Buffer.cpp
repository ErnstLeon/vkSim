#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/buffers/Buffer.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim
{

Buffer::Buffer(vksim::Device &device) : m_device(device) {};

void Buffer::create(const BufferCreateInfo &createInfo)
{
  vk::BufferCreateInfo bufferInfo{.size = createInfo.size,
                                  .usage = createInfo.usage,
                                  .sharingMode = vk::SharingMode::eExclusive};

  m_buffer = vk::raii::Buffer(m_device.logical(), bufferInfo);
  vk::MemoryRequirements memRequirements = m_buffer.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex =
          m_device.findMemoryType(memRequirements.memoryTypeBits, createInfo.properties)
              .value_or(0)};

  m_bufferMemory = vk::raii::DeviceMemory(m_device.logical(), allocInfo);
  m_buffer.bindMemory(*m_bufferMemory, 0);
}

auto Buffer::getSize() const -> vk::DeviceSize { return m_buffer.getMemoryRequirements().size; }

auto Buffer::copyFromBuffer(Buffer &buffer, uint32_t size,
                            vk::raii::CommandBuffer &commandBuffer) const -> void
{
  vk::BufferCopy copyRegion{.srcOffset = 0, .dstOffset = 0, .size = size};
  commandBuffer.copyBuffer(*buffer.getVkBuffer(), *m_buffer, copyRegion);
}

auto Buffer::getVkBuffer() const -> const vk::raii::Buffer & { return m_buffer; }

auto Buffer::getVkBufferMemory() const -> const vk::raii::DeviceMemory & { return m_bufferMemory; }

} // namespace vksim
