#pragma once

#include <deque>
#include <expected>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/render/context/CommandPool.hpp"
#include "vksim/render/context/DeviceSelector.hpp"
#include "vksim/render/device/Device.hpp"
#include "vksim/render/queue/Queue.hpp"
#include "vksim/render/window/Window.hpp"

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
 * @brief VulkanContext class encapsulates the Vulkan context and manages
 *        Vulkan resources.
 * @note The VulkanContext owns the Vulkan instance, physical device, logical device, queues,
 * command pools and surface. It is responsible for creating and managing these resources throughout
 * the lifetime of the application.
 */
class VulkanContext
{
public:
  /**
   * @brief Constructs a VulkanContext with the specified GLFW window. It
   * initializes the Vulkan instance, selects a physical device, creates a
   * logical device, and sets up the swap chain and command pool.
   * @param window Reference to the Window object.
   * @param createInfo Structure containing information for creating the
   * Vulkan context.
   */
  VulkanContext(Window &window, ContextCreateInfo createInfo);

  VulkanContext(VulkanContext *) = delete;
  auto operator=(VulkanContext *) -> VulkanContext & = delete;

  VulkanContext(VulkanContext &&) noexcept = default;
  auto operator=(VulkanContext &&) noexcept -> VulkanContext & = delete;

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
  [[nodiscard]] auto requestQueue(const QueueRequest &request) -> Queue &;

  /**
   * @brief Returns the Vulkan logical device.
   * @return Reference to the Vulkan logical device.
   */
  [[nodiscard]] auto getDevice() const -> const vksim::Device &;

  /**
   * @brief Returns the Vulkan device.
   * @return Reference to the Vulkan device.
   */
  [[nodiscard]] auto getDevice() -> vksim::Device &;

  /**
   * @brief Returns the Vulkan surface.
   * @return Reference to the Vulkan surface.
   */
  [[nodiscard]]
  auto getInstance() const -> const vk::raii::Instance &;

  /**
   * @brief Returns the Vulkan surface.
   * @return Reference to the Vulkan surface.
   */
  [[nodiscard]] auto getSurface() const -> const vk::raii::SurfaceKHR &;

  /**
   * @brief Returns the Vulkan instance.
   * @return Reference to the Vulkan instance.
   */
  [[nodiscard]] auto getWindow() const -> const vksim::Window &;

  /**
   * @brief Returns the GLFW window associated with the Vulkan context.
   * @return Pointer to the GLFW window.
   */
  [[nodiscard]]
  auto getGLFWwindow() const -> GLFWwindow *;

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
  [[nodiscard]] auto getCommandPool(uint32_t familyIndex) const -> const CommandPool &;

  /**
   * @brief Return the queue of the default graphics queue (first requested graphics queue).
   * @return Reference to the default queue.
   */
  [[nodiscard]]
  auto getDefaultGraphicsQueue() const -> const Queue &;

  /**
   * @brief Return the queue of the default compute queue (first requested compute queue).
   * @return Reference to the default queue.
   */
  [[nodiscard]]
  auto getDefaultComputeQueue() const -> const Queue &;

  /**
   * @brief Return the queue of the default transfer queue (first requested transfer queue).
   * @return Reference to the default queue.
   */
  [[nodiscard]]
  auto getDefaultTransferQueue() const -> const Queue &;

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

  // Vulkan resources and context information
  vk::raii::Context m_context;
  vksim::Window &m_window;
  vk::raii::Instance m_instance = nullptr;
  vk::raii::DebugUtilsMessengerEXT m_debugMessenger = nullptr;
  vk::raii::SurfaceKHR m_surface = nullptr;
  vksim::Device m_device;

  ContextCreateInfo m_createInfo;

  /** @brief Vector of queue requests made to the Vulkan context. */
  std::vector<QueueRequest> m_queueRequests;

  /** @brief Queue handles assigned to the logical device, each with its
   * family index, queue index, and Vulkan queue object. Owns the queue objects and ensures they
   * remain valid for the lifetime of the context. Use std::unique_ptr for queue handles to be able
   * to return stable references when adding new queues.
   */
  std::vector<std::unique_ptr<Queue>> m_queues;

  /** @brief Map of command pools, one for each queue family. The map is keyed by
   * the queue family index. Owns the command pool objects and ensures they remain valid for the
   * lifetime of the context.
   */
  std::unordered_map<uint32_t, CommandPool> m_commandPools;
};
} // namespace vksim