#pragma once

#include <cstdint>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace vksim
{

/// Forward declaration of VulkanContext class
class VulkanContext;

/** @brief Structure to hold information for creating a command pool.
 */
struct CommandPoolCreateInfo
{
  vk::CommandPoolCreateFlags flags;
  uint32_t queueFamily;
};

/** @brief Structure to hold information for allocating command buffers
 *        from a command pool.
 */
struct BufferAllocationInfo
{
  vk::CommandBufferLevel level;
  uint32_t count;
};

/** @brief CommandPool class encapsulates a Vulkan command pool and
 *        manages its associated resources.
 */
class CommandPool
{
public:
  CommandPool(const CommandPool &) = delete;
  CommandPool(CommandPool &&) noexcept = default;

  auto operator=(const CommandPool &) -> CommandPool & = delete;
  auto operator=(CommandPool &&) -> CommandPool & = delete;

  /** @brief Constructs a CommandPool with the specified create info.
   * @param createInfo Structure containing information for creating the
   * command pool.
   */
  CommandPool(VulkanContext &context, const CommandPoolCreateInfo &createInfo);

  /** @brief Allocates command buffers from the command pool.
   * @param allocInfo Structure containing information for allocating
   * command buffers.
   * @return Vector of allocated command buffers.
   */
  auto allocateCommandBuffers(const BufferAllocationInfo &allocInfo) const
      -> std::vector<vk::raii::CommandBuffer>;

  /** @brief Returns the underlying Vulkan command pool.
   * @return Reference to the Vulkan command pool.
   */
  [[nodiscard]]
  auto get() const -> const vk::raii::CommandPool &;

  /** @brief Resets the command pool, releasing all command buffers
   * allocated from it.
   * @param flags Optional flags for resetting the command pool.
   */
  void reset(vk::CommandPoolResetFlags flags = {});

private:
  vk::raii::CommandPool m_commandPool = nullptr;
  VulkanContext &m_context;
};
} // namespace vksim