#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/context/CommandPool.hpp"
#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim
{

CommandPool::CommandPool(VulkanContext &context, const CommandPoolCreateInfo &createInfo)
    : m_context(context)
{
  vk::CommandPoolCreateInfo poolInfo{.flags = createInfo.flags,
                                     .queueFamilyIndex = createInfo.queueFamily};
  m_commandPool = vk::raii::CommandPool(m_context.getDevice().logical(), poolInfo);
}

auto CommandPool::allocateCommandBuffers(const CommandBufferAllocationInfo &allocInfo) const
    -> std::vector<vk::raii::CommandBuffer>
{
  vk::CommandBufferAllocateInfo commandBufferAllocInfo{.commandPool = *m_commandPool,
                                                       .level = allocInfo.level,
                                                       .commandBufferCount = allocInfo.count};

  spdlog::info("Allocating {} command buffers from command pool", allocInfo.count);
  return vk::raii::CommandBuffers(m_context.getDevice().logical(), commandBufferAllocInfo);
}

auto CommandPool::get() const -> const vk::raii::CommandPool & { return m_commandPool; }

void CommandPool::reset(vk::CommandPoolResetFlags flags) { m_commandPool.reset(flags); }

} // namespace vksim