#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace vksim
{
/**
 * @brief Structure to represent a queue handle, including family index,
 *        queue index, and the Vulkan queue object.
 */
struct Queue
{
  uint32_t familyIndex{};
  uint32_t queueIndex{};
  vk::raii::Queue vkQueue{nullptr};

  explicit Queue(uint32_t family, uint32_t index, vk::raii::Queue queue)
      : familyIndex(family), queueIndex(index), vkQueue(std::move(queue))
  {
  }

  Queue(const Queue &) = delete;
  Queue(Queue &&) noexcept = default;

  auto operator=(const Queue &) -> Queue & = delete;
  auto operator=(Queue &&) -> Queue & = default;
};

/**
 * @brief Structure to hold information about a queue family. This will be used to query the
 * available queue families of a physical device and their properties.
 */
struct QueueFamilyInfo
{
  uint32_t index;
  uint32_t count;
  vk::QueueFlags queueFlags;
  bool supportsPresent;
};

/**
 * @brief Structure to request a specific queue with required flags and
 * present support. This will be used to request queues from the Vulkan logical device during
 * context creation.
 */
struct QueueRequest
{
  vk::QueueFlags requiredFlags;
  bool requiresPresent = false;
};

/**
 * @brief Structure to represent an assigned queue from a specific queue
 * family. This will be used to store the assigned queues after the logical device is created
 * and queues are assigned.
 */
struct QueueAssignment
{
  uint32_t familyIndex;
  uint32_t queueIndex;
};

} // namespace vksim
