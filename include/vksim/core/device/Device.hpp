#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <expected>

namespace vksim
{

/** @brief Simple wrapper for Vulkan physical and logical device handles. Owns the handles and
 * provides utility functions for querying format properties.
 */
class Device
{
public:
  Device() = default;

  Device(const Device &) = delete;
  Device(Device &&) noexcept = default;

  auto operator=(const Device &) -> Device & = delete;
  auto operator=(Device &&) noexcept -> Device & = default;

  Device(vk::raii::PhysicalDevice physicalDevice, vk::raii::Device logicalDevice);

  /** @brief Returns the physical device handle.
   */
  [[nodiscard]] auto physical() const -> const vk::raii::PhysicalDevice &;

  /** @brief Returns the logical device handle.
   */
  [[nodiscard]] auto logical() const -> const vk::raii::Device &;

  /** @brief Returns the subgroup size of the physical device.
   * @return The subgroup size, or an error message if the query fails.
   */
  [[nodiscard]]
  auto getSubgroupSize() const -> std::expected<uint32_t, std::string>;

  /** @brief Sets the physical device handle by taking ownership of the RAII object.
   */
  auto setPhysicalDevice(vk::raii::PhysicalDevice &&physicalDevice) -> void;

  /** @brief Sets the logical device handle by taking ownership of the RAII object.
   */
  auto setLogicalDevice(vk::raii::Device &&logicalDevice) -> void;

  /** @brief Find the memory type index for the given type filter and properties.
   * @param typeFilter The type filter to use for finding the memory type.
   * @param properties The required memory properties.
   * @return The index of the found memory type.
   */
  auto findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
      -> std::expected<uint32_t, std::string>;

  /** @brief Finds a supported format from a list of candidates.
   * @param device The Vulkan device.
   * @param candidates The list of candidate formats to check.
   * @param tiling The desired image tiling.
   * @param features The required format features.
   * @return The first supported format found in the candidates.
   */
  auto findSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling,
                           vk::FormatFeatureFlags features)
      -> std::expected<vk::Format, std::string>;

  /** @brief Finds a suitable depth format for the device.
   * @return The found depth format.
   */
  auto findDepthFormat() -> std::expected<vk::Format, std::string>;

private:
  vk::raii::PhysicalDevice m_physicalDevice = nullptr;
  vk::raii::Device m_logicalDevice = nullptr;
};

} // namespace vksim
