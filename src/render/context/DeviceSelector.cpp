#include <sys/types.h>
#include <unordered_map>
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/render/context/DeviceSelector.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim
{

auto DeviceSelector::pickPhysicalDevice(vk::raii::Instance const &instance,
                                        const vk::SurfaceKHR &surface,
                                        std::vector<const char *> const &deviceExtensions,
                                        DeviceFeatures const &deviceFeatures,
                                        std::vector<QueueRequest> const &requestedQueues)
    -> DeviceSelection
{
  // Iterate through all available physical devices and find the first suitable one
  std::pair<bool, DeviceSelection> result;
  std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
  auto const devIter =
      std::ranges::find_if(physicalDevices,
                           [&](auto const &physicalDevice) -> auto
                           {
                             result = isDeviceSuitable(physicalDevice, surface, deviceExtensions,
                                                       deviceFeatures, requestedQueues);
                             return result.first;
                           });

  // If no suitable device is found, log an error and abort the program
  if (devIter == physicalDevices.end())
  {
    spdlog::error("Failed to find suitable GPU!");
    std::abort();
  }

  // Log the selected physical device and its properties
  spdlog::info("Selected physical device: {}", devIter->getProperties().deviceName.data());
  spdlog::info("Physical device type: {}", vk::to_string(devIter->getProperties().deviceType));

  // Log the queue family properties of the selected physical device
  for (uint32_t i = 0; i < result.second.queueFamilies.size(); ++i)
  {
    const auto &queueFamily = result.second.queueFamilies[i];
    spdlog::info("Queue family {}: flags={}, count={}, supportsPresent={}", i,
                 vk::to_string(queueFamily.queueFlags), queueFamily.count,
                 queueFamily.supportsPresent);
  }

  return result.second;
}

auto DeviceSelector::isDeviceSuitable(const vk::raii::PhysicalDevice &physicalDevice,
                                      const vk::SurfaceKHR &surface,
                                      const std::vector<const char *> &deviceExtensions,
                                      const DeviceFeatures &deviceFeatures,
                                      const std::vector<QueueRequest> &requestedQueues)
    -> std::pair<bool, DeviceSelection>
{
  // Check if the physicalDevice supports the Vulkan 1.3 API version
  bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

  // Query queue families
  auto queueFamilies = queryQueueFamilies(physicalDevice, surface);

  // Check if all requested queues are supported by the device
  auto supportQueues = checkQueues(physicalDevice, surface, queueFamilies, requestedQueues);

  std::vector<QueueAssignment> queueAssignments;
  if (supportQueues)
  {
    queueAssignments = assignQueues(queueFamilies, requestedQueues);
  }

  // Check if all device extenstions are available
  auto supportExtensions = checkExtensions(physicalDevice, deviceExtensions);

  // Check if all features are supported
  auto [supportFeatures, features] = checkFeatures(physicalDevice, deviceFeatures);

  // Return a pair indicating whether the device is suitable and the corresponding DeviceSelection
  return {supportQueues && supportExtensions && supportFeatures,
          {.physicalDevice = physicalDevice,
           .queueFamilies = std::move(queueFamilies),
           .queueAssignments = std::move(queueAssignments),
           .featureChain = features,
           .extensions = deviceExtensions}};
}

auto DeviceSelector::queryQueueFamilies(vk::raii::PhysicalDevice const &device,
                                        vk::SurfaceKHR surface) -> std::vector<QueueFamilyInfo>
{
  // Iterate through the queue families of the physical device and query their properties, including
  // whether they support present operations on the given surface and store the information in a
  // vector of QueueFamilyInfo structures
  std::vector<QueueFamilyInfo> queueFamilies;
  auto props = device.getQueueFamilyProperties();
  for (uint32_t i = 0; i < props.size(); ++i)
  {
    QueueFamilyInfo info;
    info.index = i;
    info.queueFlags = props[i].queueFlags;
    info.count = props[i].queueCount;

    info.supportsPresent = (device.getSurfaceSupportKHR(i, surface) != 0U);

    queueFamilies.push_back(info);
  }

  return queueFamilies;
}

auto DeviceSelector::checkExtensions(vk::raii::PhysicalDevice const &device,
                                     std::vector<const char *> const &deviceExtensions) -> bool
{
  // Check if all required physicalDevice extensions are available
  auto availableDeviceExtensions = device.enumerateDeviceExtensionProperties();
  return std::ranges::all_of(
      deviceExtensions,
      [&availableDeviceExtensions](auto const &deviceExtension) -> auto
      {
        return std::ranges::any_of(
            availableDeviceExtensions,
            [deviceExtension](auto const &availableDeviceExtension) -> auto
            { return strcmp(availableDeviceExtension.extensionName, deviceExtension) == 0; });
      });
}

auto DeviceSelector::checkQueues(vk::raii::PhysicalDevice const &device,
                                 const vk::SurfaceKHR &surface,
                                 std::vector<QueueFamilyInfo> const &queueFamilies,
                                 std::vector<QueueRequest> const &requestedQueues) -> bool
{
  // Check if all requested queues are supported by the device
  auto supportQueues = std::ranges::all_of(
      requestedQueues,
      [&](auto const &qrq) -> auto
      {
        return std::ranges::any_of(queueFamilies,
                                   [&](auto const &qfm) -> auto
                                   {
                                     bool flagsOk =
                                         (qfm.queueFlags & qrq.requiredFlags) == qrq.requiredFlags;

                                     bool presentOk = !qrq.requiresPresent || qfm.supportsPresent;

                                     return flagsOk && presentOk;
                                   });
      });

  return supportQueues;
}

auto DeviceSelector::checkFeatures(vk::raii::PhysicalDevice const &device,
                                   DeviceFeatures const &deviceFeatures)
    -> std::pair<bool, vk::StructureChain<
                           vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
                           vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features,
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>>
{
  // Get the supported features of the physicalDevice, including render features and extended
  // features If new features are added to DeviceFeatures, their support should also be checked here
  auto features =
      device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
                          vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features,
                          vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

  const auto &render = features.get<vk::PhysicalDeviceFeatures2>().features;

  const auto &vk11 = features.get<vk::PhysicalDeviceVulkan11Features>();

  const auto &vk12 = features.get<vk::PhysicalDeviceVulkan12Features>();

  const auto &vk13 = features.get<vk::PhysicalDeviceVulkan13Features>();

  const auto &extDyn = features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

  // Check if all required features are supported by the physicalDevice
  // If new features are added to DeviceFeatures, they should also be checked here
  bool supportsRequiredFeatures =
      (!deviceFeatures.anisotropicFiltering || (render.samplerAnisotropy != 0U)) &&
      (!deviceFeatures.shaderDrawParameters || (vk11.shaderDrawParameters != 0U)) &&
      (!deviceFeatures.dynamicRendering || (vk13.dynamicRendering != 0U)) &&
      (!deviceFeatures.synchronization2 || (vk13.synchronization2 != 0U)) &&
      (!deviceFeatures.extendedDynamicState || (extDyn.extendedDynamicState != 0U)) &&
      (!deviceFeatures.runtimeDescriptorArray || (vk12.runtimeDescriptorArray != 0U));

  return {supportsRequiredFeatures, std::move(features)};
}

auto DeviceSelector::assignQueues(std::vector<QueueFamilyInfo> const &queueFamilies,
                                  std::vector<QueueRequest> const &requests)
    -> std::vector<QueueAssignment>
{
  std::vector<QueueAssignment> result;

  // track how many queues we already consumed per family
  std::unordered_map<uint32_t, uint32_t> usedQueuePerFamilyCount;

  // Iterate through the requested queues and assign them to available queue families based on their
  // required flags and present support. If a suitable queue family is found, the queue is assigned
  // and the used queue count for that family is incremented. If no suitable queue family is found,
  // an error is logged and the program aborts.
  for (const auto &req : requests)
  {
    bool assigned = false;
    result.emplace_back();

    // Find a queue family that satisfies the request and has available queues
    // First Check if there are families that are dedicated to the requested queue type (e.g.,
    // graphics, compute, transfer) and have available queues
    for (const auto &fam : queueFamilies)
    {
      // Check if the queue family flags equal the required flags and present support, and if it has
      // available queues
      bool flagsOk = fam.queueFlags == req.requiredFlags;
      bool presentOk = !req.requiresPresent || fam.supportsPresent;

      // Check if the queue family has available queues to satisfy the request
      uint32_t used = usedQueuePerFamilyCount[fam.index];
      bool hasCapacity = (used + 1) <= fam.count;

      // If chechs are passed and the request has not been assigned yet, push back the assignment
      // and increment the used queue count for the family
      if (flagsOk && presentOk && hasCapacity && !assigned)
      {
        // assign the queue to the result and increment the used queue count for the family
        result.back() = QueueAssignment{.familyIndex = fam.index, .queueIndex = used};
        usedQueuePerFamilyCount[fam.index] += 1;
        assigned = true;
        break;
      }
    }

    if (!assigned)
    {
      // If no dedicated queue family was found, check for any queue family that satisfies the
      // request and has available queues
      for (const auto &fam : queueFamilies)
      {
        // Check if the queue family has the required flags and present support, and if it has
        // available queues
        bool flagsOk = (fam.queueFlags & req.requiredFlags) == req.requiredFlags;
        bool presentOk = !req.requiresPresent || fam.supportsPresent;

        // Check if the queue family has available queues to satisfy the request
        uint32_t used = usedQueuePerFamilyCount[fam.index];
        bool hasCapacity = (used + 1) <= fam.count;

        // If chechs are passed and the request has not been assigned yet, push back the assignment
        // and increment the used queue count for the family
        if (flagsOk && presentOk && hasCapacity && !assigned)
        {
          // assign the queue to the result and increment the used queue count for the family
          result.back() = QueueAssignment{.familyIndex = fam.index, .queueIndex = used};
          usedQueuePerFamilyCount[fam.index] += 1;
          assigned = true;
          spdlog::info("Assigned queue request to dedicated queue family {} with index {}",
                       fam.index, used);
          break;
        }
      }
    }

    // If no suitable queue family was found for the request, log an error and abort the program
    if (!assigned)
    {
      spdlog::error("Failed to assign queue request, too many requests!");
      std::abort();
    }
  }
  return result;
}
} // namespace vksim