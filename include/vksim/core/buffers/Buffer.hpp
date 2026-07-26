#pragma once

#include <optional>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/device/Device.hpp"

namespace vksim
{

/** @brief Structure to hold information for creating a Vulkan buffer.
 */
struct BufferCreateInfo
{
  vk::DeviceSize size{0};
  vk::BufferUsageFlags usage;
  vk::MemoryPropertyFlags properties;
  std::optional<std::string> debugName; // Optional debug name for the buffer
};

/** @brief Buffer class encapsulates a Vulkan buffer and manages its
 *        associated resources. It provides methods for creating, copying, and accessing the buffer
 * and its memory. The Buffer class is responsible for allocating and freeing the buffer memory, as
 * well as providing access to the underlying Vulkan buffer and device memory objects. It must be
 * initialized with a vksim::Device reference to ensure proper resource management and lifetime
 * control.
 */
class Buffer
{
public:
  Buffer(const Buffer &) = delete;
  Buffer(Buffer &&) noexcept = default;

  auto operator=(const Buffer &) -> Buffer & = delete;
  auto operator=(Buffer &&) -> Buffer & = delete;

  /** @brief Initializes a Buffer with the specified create info.
   * @param context Reference to the Vulkan context for access to GPU resources.
   */
  Buffer(VulkanContext &context);

  /** @brief Creates a Vulkan buffer with the specified create info. This method allocates the
   * buffer and its associated memory, and binds them together. It must be called after the Buffer
   * object is constructed and before any operations are performed on the buffer. The createInfo
   * parameter specifies the size, usage, and memory properties of the buffer to be created.
   * @param createInfo Structure containing buffer creation parameters.
   */
  auto create(const BufferCreateInfo &createInfo) -> void;

  /** @brief Returns the size of the buffer in bytes.
   * @return The size of the buffer as a vk::DeviceSize.
   */
  [[nodiscard]]
  auto getSize() const -> vk::DeviceSize;

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
   * @param srcOffset The offset in the source buffer from which to start copying.
   * @param dstOffset The offset in the destination buffer where the data will be copied.
   */
  auto copyFromBuffer(Buffer &buffer, uint32_t size, vk::raii::CommandBuffer &commandBuffer,
                      uint32_t srcOffset = 0, uint32_t dstOffset = 0) const -> void;

  /** @brief Copies data from host memory to this buffer.
   * @param data Pointer to the host memory containing the data to copy.
   * @param size The size of the data to copy.
   * @param srcOffset The offset in the source host memory from which to start copying.
   * @param dstOffset The offset in the destination buffer where the data will be copied.
   */
  auto copyFromHost(const void *data, uint32_t size, uint32_t srcOffset = 0,
                    uint32_t dstOffset = 0) const -> void;

  /** @brief Copies data from this buffer to host memory.
   * @param dstData Pointer to the host memory where the data will be copied.
   * @param size The size of the data to copy.
   * @param srcOffset The offset in the buffer from which to start copying.
   * @param dstOffset The offset in the destination host memory where the data will be copied.
   */
  auto copyToHost(void *dstData, uint32_t size, uint32_t srcOffset = 0,
                  uint32_t dstOffset = 0) const -> void;

private:
  vk::raii::Buffer m_buffer = nullptr;
  vk::raii::DeviceMemory m_bufferMemory = nullptr;
  VulkanContext &m_context;
};

} // namespace vksim
