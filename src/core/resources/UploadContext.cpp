#include <cstdlib>
#include <spdlog/spdlog.h>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/context/CommandPool.hpp"
#include "vksim/core/resources/UploadContext.hpp"

namespace vksim
{

UploadContext::UploadContext(VulkanContext &context) : m_context(context) {}

auto UploadContext::begin() -> void
{
  // Allocate a command buffer from the command pool associated with the default queue
  const auto &defaultQueue = m_context.getDefaultQueue();
  const auto &commandPool = m_context.getCommandPool(defaultQueue.familyIndex);

  vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool.get(),
                                          .level = vk::CommandBufferLevel::ePrimary,
                                          .commandBufferCount = 1};
  auto commandBuffers = vk::raii::CommandBuffers(m_context.getDevice().logical(), allocInfo);
  m_commandBuffer = std::move(commandBuffers.front());

  // Begin recording commands into the command buffer
  vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  m_commandBuffer.begin(beginInfo);
}

auto UploadContext::submitAndWait() -> void
{
  if (m_commandBuffer == nullptr)
  {
    spdlog::warn("UploadContext::submitAndWait called without active command buffer");
    return;
  }

  // End recording commands into the command buffer
  m_commandBuffer.end();

  // Submit the command buffer to the default queue and wait for it to finish
  vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*m_commandBuffer};
  const auto &defaultQueue = m_context.getDefaultQueue();
  defaultQueue.queue.submit(submitInfo, nullptr);
  defaultQueue.queue.waitIdle();

  spdlog::info("UploadContext: Command buffer submitted and completed, {} staging buffers released "
               "(2 per Mesh, 1 per Texture, 1 per Material)",
               m_stagingBuffers.size());

  m_commandBuffer = nullptr;
  m_stagingBuffers.clear();
}

} // namespace vksim
