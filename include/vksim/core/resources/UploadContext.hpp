#pragma once

#include "vksim/core/context/VulkanContext.hpp"
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/buffers/Buffer.hpp"

namespace vksim
{

/** @brief UploadContext class manages the command buffer and staging buffers
 *        used for uploading resources to the GPU. This is necessary to keep the staging buffers
 * alive until the command buffer has finished executing.
 */
class UploadContext
{
public:
  UploadContext() = delete;

  /** @brief Constructs an UploadContext with a VulkanContext.
   *  @param context The VulkanContext to use for uploads.
   */
  explicit UploadContext(VulkanContext *context);

  UploadContext(const UploadContext &) = delete;
  UploadContext(UploadContext &&) noexcept = default;

  auto operator=(const UploadContext &) -> UploadContext & = delete;
  auto operator=(UploadContext &&) -> UploadContext & = default;

  /** @brief Begins the upload process by allocating a command buffer and starting the recording.
   */
  auto begin() -> void;

  /** @brief Ends the upload process by ending the command buffer recording and submitting it to the
   * GPU. It waits for the GPU to finish executing the command buffer before returning. This release
   * the command buffer and clears the staging buffers.
   */
  auto submitAndWait() -> void;

  /** @brief Returns the command buffer used for uploads.
   *  @return Reference to the command buffer.
   */
  auto getCommandBuffer() -> vk::raii::CommandBuffer & { return m_commandBuffer; }

  /** @brief Adds a staging buffer to the list of buffers to be kept alive until the upload is
   * complete.
   *  @param buffer The staging buffer to add.
   */
  auto addStagingBuffer(Buffer &&buffer) -> void { m_stagingBuffers.push_back(std::move(buffer)); }

private:
  VulkanContext *m_context = nullptr;
  vk::raii::CommandBuffer m_commandBuffer = nullptr;
  std::vector<Buffer> m_stagingBuffers;
};

} // namespace vksim
