#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace vksim
{

/**
 * @brief Structure to hold information about a queue family.
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
 *        present support.
 */
struct QueueRequest
{
  vk::QueueFlags requiredFlags;
  bool requiresPresent = false;
};

/**
 * @brief Structure to represent an assigned queue from a specific queue
 *        family.
 */
struct QueueAssignment
{
  uint32_t familyIndex;
  uint32_t queueIndex;
};

/**
 * @brief Structure to hold information about required device features.
 * This structure can be extended to include more features as needed.
 */
struct DeviceFeatures
{
  bool anisotropicFiltering = true;
  bool shaderDrawParameters = true;
  bool dynamicRendering = true;
  bool synchronization2 = true;
  bool extendedDynamicState = true;
  bool runtimeDescriptorArray = true;
};

/**
 * @brief Structure to hold the selection of a physical device, including
 *        its queue families, assigned queues, feature chain, and
 *        extensions.
 */
struct DeviceSelection
{
  /** @brief The selected physical device.
   */
  vk::raii::PhysicalDevice physicalDevice = nullptr;
  /** @brief The queue families of the selected physical device.
   */
  std::vector<QueueFamilyInfo> queueFamilies;
  /** @brief The assigned queues (family and indices) for the requested
   * queue configurations.
   */
  std::vector<QueueAssignment> queueAssignments;

  /** @brief The feature chain of the selected physical device.
   */
  vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
                     vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features,
                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
      featureChain;
  /** @brief The required device extensions for the selected physical
   *        device.
   */
  std::vector<const char *> extensions;
};

/**
 * @brief DeviceSelector class provides functionality to select a suitable
 *        physical device based on required features, extensions, and queue
 *        requests.
 */
class DeviceSelector
{
public:
  static auto pickPhysicalDevice(vk::raii::Instance const &instance, const vk::SurfaceKHR &surface,
                                 std::vector<const char *> const &deviceExtensions,
                                 DeviceFeatures const &deviceFeatures,
                                 std::vector<QueueRequest> const &requestedQueues)
      -> DeviceSelection;

private:
  /**
   * @brief Checks if a physical device is suitable based on required
   *        extensions, features, and queue requests.
   * @param device The physical device to check.
   * @param surface The Vulkan surface for present support checks.
   * @param deviceExtensions Required device extensions.
   * @param deviceFeatures Required device features.
   * @param requestedQueues Requested queue configurations.
   * @return A pair containing a boolean indicating suitability and the
   *         corresponding DeviceSelection if suitable.
   */
  static auto isDeviceSuitable(const vk::raii::PhysicalDevice &device,
                               const vk::SurfaceKHR &surface,
                               const std::vector<const char *> &deviceExtensions,
                               const DeviceFeatures &deviceFeatures,
                               const std::vector<QueueRequest> &requestedQueues)
      -> std::pair<bool, DeviceSelection>;

  /**
   * @brief Queries the queue families of a physical device and checks for
   *        present support.
   * @param device The physical device to query.
   * @param surface The Vulkan surface for present support checks.
   * @return A vector of QueueFamilyInfo structures representing the queue
   *         families of the device.
   */
  static auto queryQueueFamilies(vk::raii::PhysicalDevice const &device, vk::SurfaceKHR surface)
      -> std::vector<QueueFamilyInfo>;

  /**
   * @brief Checks if the required device extensions are supported by the
   *        physical device.
   * @param device The physical device to check.
   * @param deviceExtensions Required device extensions.
   * @return True if all required extensions are supported, false
   * otherwise.
   */
  static auto checkExtensions(vk::raii::PhysicalDevice const &device,
                              std::vector<const char *> const &deviceExtensions) -> bool;

  /**
   * @brief Checks if the required device features are supported by the
   *        physical device.
   * @param device The physical device to check.
   * @param deviceFeatures Required device features.
   * @return A pair containing a boolean indicating if all required
   * features are supported and the corresponding feature chain.
   */
  static auto checkFeatures(vk::raii::PhysicalDevice const &device,
                            DeviceFeatures const &deviceFeatures)
      -> std::pair<bool, vk::StructureChain<
                             vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
                             vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features,
                             vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>>;

  /**
   * @brief Checks if the requested queues can be satisfied by the
   * available queue families.
   * @param queueFamilies Available queue families of the physical device.
   * @param requestedQueues Requested queue configurations.
   * @return True if all requested queues can be satisfied, false
   * otherwise.
   */
  static auto checkQueues(vk::raii::PhysicalDevice const &device, const vk::SurfaceKHR &surface,
                          std::vector<QueueFamilyInfo> const &queueFamilies,
                          std::vector<QueueRequest> const &requestedQueues) -> bool;
  /**
  * @brief Assigns queues from available queue families to satisfy the
  requested queue configurations.
  * @param queueFamilies Available queue families of the physical device.
  * @param requestedQueues Requested queue configurations.
  * @return A vector of QueueAssignment structures representing the
  * assigned queues. For each requested queue, the corresponding family index
  * and queue index are provided.
   */
  static auto assignQueues(std::vector<QueueFamilyInfo> const &queueFamilies,
                           std::vector<QueueRequest> const &requests)
      -> std::vector<QueueAssignment>;
};

} // namespace vksim