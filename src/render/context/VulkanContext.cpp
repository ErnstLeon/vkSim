#include <ranges>
#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/render/context/DeviceSelector.hpp"
#include "vksim/render/context/VulkanContext.hpp"
#include "vksim/render/device/Device.hpp"
#include "vksim/render/window/Window.hpp"
#include "vksim/utility/Error.hpp"
#include "vksim/utility/Logging.hpp"

#ifdef DEBUG
constexpr bool enableValidationLayers = true;
#else
constexpr bool enableValidationLayers = false;
#endif

namespace vksim
{

VulkanContext::VulkanContext(vksim::Window &window, ContextCreateInfo createInfo)
    : m_createInfo(std::move(createInfo)), m_window(window)
{
  // Request (if possible) dedicated queues for graphics, compute and transfer operations. The
  // requested queues will be stored in m_queueRequests and assigned during build() when the logical
  // device is created. After that user can request additional queues using requestQueue() and they
  // will be assigned during build().
  m_queueRequests.push_back(
      {.requiredFlags = vk::QueueFlagBits::eGraphics, .requiresPresent = true});
  m_queues.emplace_back(std::make_unique<Queue>(0, 0, nullptr));

  m_queueRequests.push_back(
      {.requiredFlags = vk::QueueFlagBits::eCompute, .requiresPresent = false});
  m_queues.emplace_back(std::make_unique<Queue>(0, 0, nullptr));

  m_queueRequests.push_back(
      {.requiredFlags = vk::QueueFlagBits::eTransfer, .requiresPresent = false});
  m_queues.emplace_back(std::make_unique<Queue>(0, 0, nullptr));
}

auto VulkanContext::build() -> void
{
  // Create the Vulkan instance and set up the debug messenger if validation layers are enabled
  createInstance(m_createInfo);
  setupDebugMessenger();

  // Create the Vulkan surface for the window
  createSurface();

  // Select a physical device and move it into m_physicalDevice.
  auto deviceSelection =
      DeviceSelector::pickPhysicalDevice(m_instance, m_surface, m_createInfo.device.extensions,
                                         m_createInfo.device.features, m_queueRequests);
  m_device.setPhysicalDevice(std::move(deviceSelection.physicalDevice));

  // Create the logical device and move it into m_logicalDevice. The logical device will be created
  // with the requested features, extensions, and queues. The assigned queues will be stored in
  // m_queues and can be accessed via the handle obtained from requestQueue() after build().
  createLogicalDevice(deviceSelection);

  // Create a command pool for each queue family used by the logical device. The command pools will
  // be stored in m_commandPools and can be accessed via getCommandPool() after build().
  createCommandPool();
}

void VulkanContext::createInstance(const ContextCreateInfo &createInfo)
{
  // Create the Vulkan instance with the specified application info.
  vk::ApplicationInfo appInfo{.pApplicationName = createInfo.instance.appName.c_str(),
                              .applicationVersion = createInfo.instance.appVersion,
                              .pEngineName = createInfo.instance.engineName.c_str(),
                              .engineVersion = createInfo.instance.engineVersion,
                              .apiVersion = createInfo.instance.apiVersion};

  // Check if the required layers are supported by the Vulkan
  // implementation.
  std::vector<char const *> requiredLayers;
  if (enableValidationLayers)
  {
    requiredLayers.assign(createInfo.instance.layers.begin(), createInfo.instance.layers.end());
  }

  // Check if the required validation layers are supported by the Vulkan implementation.
  auto layerProperties = m_context.enumerateInstanceLayerProperties();
  auto unsupportedLayerIt =
      std::ranges::find_if(requiredLayers,
                           [&layerProperties](auto const &requiredLayer) -> auto
                           {
                             return std::ranges::none_of(
                                 layerProperties, [requiredLayer](auto const &layerProperty) -> auto
                                 { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
                           });
  if (unsupportedLayerIt != requiredLayers.end())
  {
    spdlog::error("Required unsupported Validation Layer{}", *unsupportedLayerIt);
    std::abort();
  }

  // Check if the required extensions are supported by the Vulkan
  // implementation.
  auto requiredExtensions = createInfo.instance.extensions;

  // Append glfw required extensions
  uint32_t glfwExtensionCount = 0;
  auto *glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  requiredExtensions.insert(requiredExtensions.end(), glfwExtensions,
                            glfwExtensions + glfwExtensionCount);

  // Append Validation Layer extension if needed
  if (enableValidationLayers)
  {
    requiredExtensions.push_back(vk::EXTDebugUtilsExtensionName);
  }

  // Check if the required extensions are supported by the Vulkan implementation.
  auto extensionProperties = m_context.enumerateInstanceExtensionProperties();
  auto unsupportedPropertyIt = std::ranges::find_if(
      requiredExtensions,
      [&extensionProperties](auto const &requiredExtension) -> auto
      {
        return std::ranges::none_of(
            extensionProperties, [requiredExtension](auto const &extensionProperty) -> auto
            { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; });
      });

  if (unsupportedPropertyIt != requiredExtensions.end())
  {
    spdlog::error("Required unsupported Instance Extensions");
    std::abort();
  }

  // Create Instance
  vk::InstanceCreateInfo instanceCreateInfo{
      .flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
      .ppEnabledLayerNames = requiredLayers.data(),
      .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
      .ppEnabledExtensionNames = requiredExtensions.data()};
  m_instance = vk::raii::Instance(m_context, instanceCreateInfo);
}

auto VulkanContext::createSurface() -> void
{
  VkSurfaceKHR _surface;
  if (glfwCreateWindowSurface(*m_instance, m_window.getGLFWwindow(), nullptr, &_surface) != 0)
  {
    spdlog::error("glfwCreateWindowSurface failed with error code {}", glfwGetError(nullptr));
    std::abort();
  }
  m_surface = vk::raii::SurfaceKHR(m_instance, _surface);
}

auto VulkanContext::createLogicalDevice(const DeviceSelection &deviceSelection) -> void
{
  // find the index of the first queue family that supports graphics
  std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
      m_device.physical().getQueueFamilyProperties();

  // Based on queue assignments in DeviceSelector, count the number of
  // queues needed for each queue family
  std::unordered_map<uint32_t, uint32_t> queueFamilyCounts;
  for (const auto &request : deviceSelection.queueAssignments)
  {
    queueFamilyCounts[request.familyIndex]++;
  }

  auto featureChain = deviceSelection.featureChain;
  auto deviceExtensions = deviceSelection.extensions;

  // create a Queue create info for each queue family that has at least
  // one queue assigned
  // TO DO: Work on priority assignment for queues, currently all queues
  // have the same priority
  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
  queueCreateInfos.reserve(queueFamilyCounts.size());
  constexpr float queuePriority = 1.0F;

  // Keep per-family priority arrays alive until vk::Device creation.
  std::vector<std::vector<float>> queuePrioritiesPerFamily;
  queuePrioritiesPerFamily.reserve(queueFamilyCounts.size());

  // Create a queue create info for each queue family
  for (const auto &[familyIndex, count] : queueFamilyCounts)
  {
    queuePrioritiesPerFamily.emplace_back(count, queuePriority);
    vk::DeviceQueueCreateInfo queueCreateInfo{.queueFamilyIndex = familyIndex,
                                              .queueCount = count,
                                              .pQueuePriorities =
                                                  queuePrioritiesPerFamily.back().data()};
    queueCreateInfos.push_back(queueCreateInfo);
  }

  // Create the logical device with the specified features, extensions, and queues. The logical
  // device will be stored in m_device and can be accessed via getDevice() after build().
  vk::DeviceCreateInfo deviceCreateInfo{
      .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
      .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos = queueCreateInfos.data(),
      .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
      .ppEnabledExtensionNames = deviceExtensions.data()};

  m_device.setLogicalDevice(vk::raii::Device(m_device.physical(), deviceCreateInfo));

  // The queue assignments are stored in deviceSelection.queueAssignments (familyIndex and
  // queueIndex) and correspond to the order of the requested queues in m_queueRequests and are now
  // assigned to the m_queues vector.
  uint32_t queueIndex = 0;
  for (const auto &request : deviceSelection.queueAssignments)
  {
    m_queues[queueIndex]->familyIndex = request.familyIndex;
    m_queues[queueIndex]->queueIndex = request.queueIndex;
    m_queues[queueIndex]->vkQueue =
        m_device.logical().getQueue(request.familyIndex, request.queueIndex);

    // Log the assigned queue information
    spdlog::info("Assigned queue {}: familyIndex={}, queueIndex={}", queueIndex,
                 request.familyIndex, request.queueIndex);
    queueIndex++;
  }
}

auto VulkanContext::createCommandPool() -> void
{
  // Create a command pool for each queue family used by the logical device
  for (const auto &queueHandle : m_queues)
  {
    CommandPoolCreateInfo poolInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                   .queueFamily = queueHandle->familyIndex};
    m_commandPools.emplace(queueHandle->familyIndex, CommandPool(*this, poolInfo));

    spdlog::info("Created command pool for queue family index {}", queueHandle->familyIndex);
  }
}

VKAPI_ATTR auto VKAPI_CALL VulkanContext::debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) -> vk::Bool32
{
  auto error =
      vksim::error::Error(vksim::error::ErrorCode::VulkanValidationError, pCallbackData->pMessage);

  if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
  {
    spdlog::error(error.toString());
  }
  else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
  {
    spdlog::warn(error.toString());
  }
  else
  {
    spdlog::info(error.toString());
  }

  return vk::False;
}

auto VulkanContext::setupDebugMessenger() -> void
{
  if (!enableValidationLayers)
  {
    return;
  }

  vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
  vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
  vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
      .messageSeverity = severityFlags,
      .messageType = messageTypeFlags,
      .pfnUserCallback = &debugCallback};
  m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
}

auto VulkanContext::requestQueue(const QueueRequest &request) -> Queue &
{
  // Store the request for later processing during build()
  m_queueRequests.push_back(request);

  // Return a reference to the newly added queue. The actual queue will be assigned during
  // build() when the logical device is created.
  m_queues.emplace_back(std::make_unique<Queue>(0, 0, nullptr));

  return *m_queues.back();
}

auto VulkanContext::getCommandPool(uint32_t familyIndex) const -> const CommandPool &
{
  auto poolIt = m_commandPools.find(familyIndex);
  if (poolIt == m_commandPools.end())
  {
    spdlog::error("Command pool for queue family index {} not found", familyIndex);
    std::abort();
  }

  return poolIt->second;
}

auto VulkanContext::getDefaultGraphicsQueue() const -> const Queue &
{
  // Return a reference to the first requested queue (added in build()) which is the default
  // graphics queue.
  return *m_queues[0];
}

auto VulkanContext::getDefaultComputeQueue() const -> const Queue &
{
  // Return a reference to the second requested queue (added in build()) which is the default
  // compute queue.
  return *m_queues[1];
}

auto VulkanContext::getDefaultTransferQueue() const -> const Queue &
{
  // Return a reference to the third requested queue (added in build()) which is the default
  // transfer queue.
  return *m_queues[2];
}

auto VulkanContext::getDevice() -> vksim::Device & { return m_device; }

auto VulkanContext::getDevice() const -> const vksim::Device & { return m_device; }

auto VulkanContext::getInstance() const -> const vk::raii::Instance & { return m_instance; }

auto VulkanContext::getSurface() const -> const vk::raii::SurfaceKHR & { return m_surface; }

auto VulkanContext::getWindow() const -> const vksim::Window & { return m_window; }

auto VulkanContext::getMaxUsableSampleCount() const -> vk::SampleCountFlagBits
{
  vk::PhysicalDeviceProperties physicalDeviceProperties = m_device.physical().getProperties();

  vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
                                physicalDeviceProperties.limits.framebufferDepthSampleCounts;
  if (counts & vk::SampleCountFlagBits::e64)
  {
    return vk::SampleCountFlagBits::e64;
  }
  if (counts & vk::SampleCountFlagBits::e32)
  {
    return vk::SampleCountFlagBits::e32;
  }
  if (counts & vk::SampleCountFlagBits::e16)
  {
    return vk::SampleCountFlagBits::e16;
  }
  if (counts & vk::SampleCountFlagBits::e8)
  {
    return vk::SampleCountFlagBits::e8;
  }
  if (counts & vk::SampleCountFlagBits::e4)
  {
    return vk::SampleCountFlagBits::e4;
  }
  if (counts & vk::SampleCountFlagBits::e2)
  {
    return vk::SampleCountFlagBits::e2;
  }

  return vk::SampleCountFlagBits::e1;
}

} // namespace vksim