#pragma once

#include "vulkan/vulkan.hpp"
#include <expected>
#include <string>
#include <unordered_map>
#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/commands/CommandPool.hpp"
#include "vksim/core/context/DeviceSelector.hpp"

namespace vksim
{

/**
 * @brief Structure to hold information for creating a Vulkan instance. Name, version, layers, and
 * extensions can be specified.
 */
struct InstanceCreateInfo
{
  std::string appName;
  uint32_t appVersion = VK_MAKE_VERSION(1, 0, 0);
  std::string engineName;
  uint32_t engineVersion = VK_MAKE_VERSION(1, 0, 0);
  uint32_t apiVersion = VK_API_VERSION_1_0;

  std::vector<const char *> layers;
  std::vector<const char *> extensions;
};

/**
 * @brief Structure to hold information for creating a Vulkan logical
 * device. Extensions and features can be specified.
 */
struct DeviceCreateInfo
{
  std::vector<const char *> extensions;
  DeviceFeatures features;
};

/**
 * @brief Structure to hold information for creating a Vulkan context,
 * including instance and device creation info.
 */
struct ContextCreateInfo
{
  InstanceCreateInfo instance;
  DeviceCreateInfo device;
};

/**
 * @brief Structure to represent a queue handle, including family index,
 *        queue index, and the Vulkan queue object.
 */
struct QueueHandle
{
  uint32_t familyIndex{};
  uint32_t queueIndex{};
  vk::raii::Queue queue{nullptr};

  explicit QueueHandle(uint32_t family, uint32_t index, vk::raii::Queue queue)
      : familyIndex(family), queueIndex(index), queue(std::move(queue))
  {
  }

  QueueHandle(const QueueHandle &) = delete;
  QueueHandle(QueueHandle &&) noexcept = default;

  auto operator=(const QueueHandle &) -> QueueHandle & = delete;
  auto operator=(QueueHandle &&) -> QueueHandle & = default;
};

/**
 * @brief VulkanContext class encapsulates the Vulkan context and manages
 *        Vulkan resources.
 */
class VulkanContext
{
public:
  VulkanContext() = default;

  /**
   * @brief Constructs a VulkanContext with the specified GLFW window. It
   * initializes the Vulkan instance, selects a physical device, creates a
   * logical device, and sets up the swap chain and command pool.
   * @param window Pointer to the GLFW window.
   * @param createInfo Structure containing information for creating the
   * Vulkan
   */
  VulkanContext(GLFWwindow *window, ContextCreateInfo createInfo);

  ~VulkanContext();
  VulkanContext(VulkanContext *) = delete;
  auto operator=(VulkanContext *) -> VulkanContext & = delete;

  VulkanContext(VulkanContext &&) noexcept = default;
  auto operator=(VulkanContext &&) noexcept -> VulkanContext & = default;

  /**
   * @brief Builds the Vulkan context based on the provided create info by creating the Vulkan
   * instance, selecting a physical device, creating a logical device, and the required queues and
   * command pools.
   */
  auto build() -> void;

  /**
   * @brief Requests a queue from the Vulkan logical device based on the
   *        specified queue request. The queue is stored in the m_queues map
   *        with the request ID as the key.
   * @param request Structure containing information for requesting a queue.
   * @return Returns a reference to the requested queue handle.
   */
  auto requestQueue(const QueueRequest &request) -> QueueHandle &;

  /**
   * @brief Returns the Vulkan logical device.
   * @return Reference to the Vulkan logical device.
   */
  [[nodiscard]] auto getDevice() const -> const vk::raii::Device &;

  /**
   * @brief Returns the Vulkan physical device.
   * @return Reference to the Vulkan physical device.
   */
  [[nodiscard]] auto getPhysicalDevice() const -> const vk::raii::PhysicalDevice &;

  /**
   * @brief Returns the Vulkan surface.
   * @return Reference to the Vulkan surface.
   */
  [[nodiscard]] auto getSurface() const -> const vk::raii::SurfaceKHR &;

  /**
   * @brief Returns the highest MSAA sample count supported for both color and depth attachments.
   */
  [[nodiscard]] auto getMaxUsableSampleCount() const -> vk::SampleCountFlagBits;

  /**
   * @brief Returns the command pool associated with the specified queue
   * family index.
   * @param familyIndex The index of the queue family.
   * @return Reference to the Vulkan command pool associated with the
   * queue family index.
   */
  [[nodiscard]] auto getCommandPool(uint32_t familyIndex) -> CommandPool &;

  /**
   * @brief Return the queue handle of the default queue (first requested queue).
   * @return Reference to the default queue handle.
   */
  [[nodiscard]]
  auto getDefaultQueue() const -> const QueueHandle &;

private:
  /**
   * @brief Creates the Vulkan instance.
   * @param createInfo Structure containing information for creating the
   * Vulkan instance.
   */
  auto createInstance(const ContextCreateInfo &createInfo) -> void;

  /**
   * @brief Sets up the debug messenger.
   */
  auto setupDebugMessenger() -> void;

  /**
   * @brief DebugCallback for debug messenger.
   * @param severity Severity of the message.
   * @param type Type of the message.
   * @param pCallbackData Pointer to the callback data.
   * @param pUserData Pointer to user data.
   * @return VK_TRUE if the message should be handled, VK_FALSE otherwise.
   */
  static VKAPI_ATTR auto VKAPI_CALL debugCallback(
      vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
      const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) -> vk::Bool32;

  /**
   * @brief Creates the Vulkan surface.
   */
  auto createSurface() -> void;

  /**
   *@brief Creates the logical device and queues based on the selected
   * physical device and requested features, extensions, and queues.
   * @param deviceSelection Structure containing information for creating
   * the logical device.
   */
  auto createLogicalDevice(const DeviceSelection &deviceSelection) -> void;

  /** @brief Creates a command pool for each queue family used by the logical device. */
  auto createCommandPool() -> void;

  /** @brief Vector of queue requests made to the Vulkan context. */
  std::vector<QueueRequest> m_queueRequests;

  /** @brief Vector of queues assigned to the logical device, each with its
   * family index, queue index, and Vulkan queue object. Owns the queue objects and ensures they
   * remain valid for the lifetime of the context. */
  std::vector<QueueHandle> m_queues;

  /** @brief Map of command pools, one for each queue family. The map is keyed by
   * the queue family index. Owns the command pool objects and ensures they remain valid for the
   * lifetime of the context.
   */
  std::unordered_map<uint32_t, CommandPool> m_commandPools;

  ContextCreateInfo m_createInfo;
  GLFWwindow *m_window = nullptr;

  vk::raii::Context m_context;
  vk::raii::Instance m_instance = nullptr;
  vk::raii::PhysicalDevice m_physicalDevice = nullptr;
  vk::raii::Device m_device = nullptr;
  vk::raii::SurfaceKHR m_surface = nullptr;
  vk::raii::DebugUtilsMessengerEXT m_debugMessenger = nullptr;
};
} // namespace vksim