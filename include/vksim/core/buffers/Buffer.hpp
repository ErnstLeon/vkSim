#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/context/VulkanContext.hpp"

namespace vksim
{

/** @brief Structure to hold information for creating a Vulkan buffer.
 */
struct BufferCreateInfo
{
  vk::DeviceSize size;
  vk::BufferUsageFlags usage;
  vk::MemoryPropertyFlags properties;
};

/** @brief Buffer class encapsulates a Vulkan buffer and manages its
 *        associated resources.
 */
class Buffer
{
public:
  Buffer() = default;
  Buffer(const Buffer &) = delete;
  Buffer(Buffer &&) noexcept = default;

  auto operator=(const Buffer &) -> Buffer & = delete;
  auto operator=(Buffer &&) -> Buffer & = default;

  /** @brief Constructs a Buffer with the specified create info.
   * @param context Pointer to the Vulkan context for access to GPU resources.
   * @param createInfo Structure containing information for creating the
   * buffer.
   */
  Buffer(VulkanContext *context, const BufferCreateInfo &createInfo);

  /** @brief Returns the underlying Vulkan buffer.
   * @return Reference to the Vulkan buffer.
   */
  [[nodiscard]] auto getVkBuffer() const -> const vk::raii::Buffer &;

  /** @brief Returns the memory allocated for the buffer.
   * @return Reference to the device memory.
   */
  [[nodiscard]] auto getVkBufferMemory() const -> const vk::raii::DeviceMemory &;

  /** @brief Copies data from another buffer to this buffer.
   * @param buffer The source buffer to copy data from.
   * @param size The size of the data to copy.
   * @param commandBuffer The command buffer to record the copy commands.
   */
  auto copyFromBuffer(Buffer &buffer, uint32_t size, vk::raii::CommandBuffer &commandBuffer) const
      -> void;

private:
  /** @brief Finds a suitable memory type for the buffer.
   * @param context The Vulkan context.
   * @param typeFilter A bitmask specifying the acceptable memory types.
   * @param properties The desired memory properties.
   * @return The index of the suitable memory type.
   */
  static auto findMemoryType(VulkanContext *context, uint32_t typeFilter,
                             vk::MemoryPropertyFlags properties) -> uint32_t;

  vk::raii::Buffer m_buffer = nullptr;
  vk::raii::DeviceMemory m_bufferMemory = nullptr;
  VulkanContext *m_context = nullptr;
};

} // namespace vksim
