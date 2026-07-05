#include <ranges>
#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/context/DeviceSelector.hpp"
#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/utility/Error.hpp"
#include "vksim/utility/Logging.hpp"

#ifdef DEBUG
constexpr bool enableValidationLayers = true;
#else
constexpr bool enableValidationLayers = false;
#endif

namespace vksim
{

VulkanContext::VulkanContext(GLFWwindow *window, ContextCreateInfo createInfo)
    : m_createInfo(std::move(createInfo)), m_window(window)
{
}

VulkanContext::~VulkanContext() { m_commandPools.clear(); }

auto VulkanContext::build() -> void
{
  createInstance(m_createInfo);
  setupDebugMessenger();
  createSurface();

  // Select a physical device and move it into m_physicalDevice. Also create a logical device and
  // store it in m_device.
  auto deviceSelection =
      DeviceSelector::pickPhysicalDevice(m_instance, m_surface, m_createInfo.device.extensions,
                                         m_createInfo.device.features, m_queueRequests);
  m_physicalDevice = std::move(deviceSelection.physicalDevice);

  createLogicalDevice(deviceSelection);
  createCommandPool();
}

auto VulkanContext::requestQueue(const QueueRequest &request) -> QueueHandle &
{
  // Store the request for later processing during build()
  m_queueRequests.push_back(request);

  // Return a reference to the newly added queue handle. The actual queue will be assigned during
  // build() when the logical device is created.
  m_queues.emplace_back(0, 0, nullptr);

  return m_queues.back();
}

auto VulkanContext::getCommandPool(uint32_t familyIndex) -> CommandPool &
{
  auto poolIt = m_commandPools.find(familyIndex);
  if (poolIt == m_commandPools.end())
  {
    spdlog::error("Command pool for queue family index {} not found", familyIndex);
    std::abort();
  }

  return poolIt->second;
}

auto VulkanContext::getDefaultQueue() const -> const QueueHandle &
{
  if (m_queues.empty())
  {
    spdlog::error("No queues available to get default queue");
    std::abort();
  }
  return m_queues[0];
}

auto VulkanContext::getDevice() const -> const vk::raii::Device & { return m_device; }

auto VulkanContext::getPhysicalDevice() const -> const vk::raii::PhysicalDevice &
{
  return m_physicalDevice;
}

auto VulkanContext::getSurface() const -> const vk::raii::SurfaceKHR & { return m_surface; }

auto VulkanContext::getMaxUsableSampleCount() const -> vk::SampleCountFlagBits
{
  vk::PhysicalDeviceProperties physicalDeviceProperties = m_physicalDevice.getProperties();

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

void VulkanContext::createInstance(const ContextCreateInfo &createInfo)
{
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

  // Append Validation Layser extension if needed
  if (enableValidationLayers)
  {
    requiredExtensions.push_back(vk::EXTDebugUtilsExtensionName);
  }

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

  // Create Instace
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
  if (glfwCreateWindowSurface(*m_instance, m_window, nullptr, &_surface) != 0)
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
      m_physicalDevice.getQueueFamilyProperties();

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

  for (const auto &[familyIndex, count] : queueFamilyCounts)
  {
    queuePrioritiesPerFamily.emplace_back(count, queuePriority);
    vk::DeviceQueueCreateInfo queueCreateInfo{.queueFamilyIndex = familyIndex,
                                              .queueCount = count,
                                              .pQueuePriorities =
                                                  queuePrioritiesPerFamily.back().data()};
    queueCreateInfos.push_back(queueCreateInfo);
  }

  vk::DeviceCreateInfo deviceCreateInfo{
      .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
      .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos = queueCreateInfos.data(),
      .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
      .ppEnabledExtensionNames = deviceExtensions.data()};

  m_device = vk::raii::Device(m_physicalDevice, deviceCreateInfo);

  uint32_t queueIndex = 0;
  for (const auto &request : deviceSelection.queueAssignments)
  {
    m_queues[queueIndex].familyIndex = request.familyIndex;
    m_queues[queueIndex].queueIndex = request.queueIndex;
    m_queues[queueIndex].queue = m_device.getQueue(request.familyIndex, request.queueIndex);

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
                                   .queueFamily = queueHandle.familyIndex};
    m_commandPools.emplace(queueHandle.familyIndex, CommandPool(this, poolInfo));

    spdlog::info("Created command pool for queue family index {}", queueHandle.familyIndex);
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

} // namespace vksim