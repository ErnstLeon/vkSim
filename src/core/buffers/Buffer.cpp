#include <cstdint>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/buffers/Buffer.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim
{

Buffer::Buffer(VulkanContext &context) : m_context(context) {};

void Buffer::create(const BufferCreateInfo &createInfo)
{
  vk::BufferCreateInfo bufferInfo{.size = createInfo.size,
                                  .usage = createInfo.usage,
                                  .sharingMode = vk::SharingMode::eExclusive};

  m_buffer = vk::raii::Buffer(m_context.getDevice().logical(), bufferInfo);
  vk::MemoryRequirements memRequirements = m_buffer.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = m_context.getDevice()
                             .findMemoryType(memRequirements.memoryTypeBits, createInfo.properties)
                             .value_or(0)};

  m_bufferMemory = vk::raii::DeviceMemory(m_context.getDevice().logical(), allocInfo);
  m_buffer.bindMemory(*m_bufferMemory, 0);

  if (createInfo.debugName.has_value() && DEBUG)
  {
    vk::DebugUtilsObjectNameInfoEXT nameInfo{
        .objectType = vk::ObjectType::eBuffer,
        .objectHandle = reinterpret_cast<uint64_t>(static_cast<VkBuffer>(*m_buffer)),
        .pObjectName = createInfo.debugName->c_str()};

    m_context.getDevice().logical().setDebugUtilsObjectNameEXT(nameInfo);
  }
}

auto Buffer::getSize() const -> vk::DeviceSize { return m_buffer.getMemoryRequirements().size; }

auto Buffer::copyFromBuffer(Buffer &buffer, uint32_t size, vk::raii::CommandBuffer &commandBuffer,
                            uint32_t srcOffset, uint32_t dstOffset) const -> void
{
  vk::BufferCopy copyRegion{.srcOffset = srcOffset, .dstOffset = dstOffset, .size = size};
  commandBuffer.copyBuffer(*buffer.getVkBuffer(), *m_buffer, copyRegion);
}

auto Buffer::copyFromHost(const void *data, uint32_t size, uint32_t srcOffset,
                          uint32_t dstOffset) const -> void
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
  vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  commandBuffers.front().begin(beginInfo);

  // create staging buffer for host-visible memory
  Buffer stagingBuffer(m_context);
  stagingBuffer.create(BufferCreateInfo{.size = size,
                                        .usage = vk::BufferUsageFlagBits::eTransferSrc,
                                        .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                                      vk::MemoryPropertyFlagBits::eHostCoherent});

  // Map the staging buffer memory, copy data from host to the mapped memory, and unmap it
  auto *mappedMemory = stagingBuffer.getVkBufferMemory().mapMemory(0, size);
  std::memcpy(mappedMemory, static_cast<const char *>(data) + srcOffset, size);
  stagingBuffer.getVkBufferMemory().unmapMemory();

  // Copy data from the staging buffer to the destination buffer using the command buffer
  copyFromBuffer(stagingBuffer, size, commandBuffers.front(), 0, dstOffset);

  // End recording commands into the command buffer
  commandBuffers.front().end();

  // Submit the command buffer to the default transfer queue for execution and wait for completion
  vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandBuffers.front()};
  defaultQueue.vkQueue.submit(submitInfo, nullptr);
  defaultQueue.vkQueue.waitIdle();
}

auto Buffer::copyToHost(void *dstData, uint32_t size, uint32_t srcOffset, uint32_t dstOffset) const
    -> void
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
  vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  commandBuffers.front().begin(beginInfo);

  // create staging buffer for host-visible memory
  Buffer stagingBuffer(m_context);
  stagingBuffer.create(BufferCreateInfo{.size = size,
                                        .usage = vk::BufferUsageFlagBits::eTransferDst,
                                        .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                                      vk::MemoryPropertyFlagBits::eHostCoherent});

  // Copy data from the source buffer to the staging buffer using the command buffer
  stagingBuffer.copyFromBuffer(const_cast<Buffer &>(*this), size, commandBuffers.front(), srcOffset,
                               0);

  // End recording commands into the command buffer
  commandBuffers.front().end();

  // Submit the command buffer to the default transfer queue for execution and wait for completion
  vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandBuffers.front()};
  defaultQueue.vkQueue.submit(submitInfo, nullptr);
  defaultQueue.vkQueue.waitIdle();

  // Map the staging buffer memory, copy data from the mapped memory to host, and unmap it
  auto *mappedMemory = stagingBuffer.getVkBufferMemory().mapMemory(0, size);
  std::memcpy(static_cast<char *>(dstData) + dstOffset, mappedMemory, size);
  stagingBuffer.getVkBufferMemory().unmapMemory();
}

auto Buffer::getVkBuffer() const -> const vk::raii::Buffer & { return m_buffer; }

auto Buffer::getVkBufferMemory() const -> const vk::raii::DeviceMemory & { return m_bufferMemory; }

} // namespace vksim
