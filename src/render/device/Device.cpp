#include "vksim/render/device/Device.hpp"

#include <cstdlib>

#include <spdlog/spdlog.h>

namespace vksim
{

Device::Device(vk::raii::PhysicalDevice physicalDevice, vk::raii::Device logicalDevice)
    : m_physicalDevice(std::move(physicalDevice)), m_logicalDevice(std::move(logicalDevice))
{
}

auto Device::physical() const -> const vk::raii::PhysicalDevice & { return m_physicalDevice; }

auto Device::logical() const -> const vk::raii::Device & { return m_logicalDevice; }

auto Device::setPhysicalDevice(vk::raii::PhysicalDevice &&physicalDevice) -> void
{
  m_physicalDevice = std::move(physicalDevice);
}

auto Device::setLogicalDevice(vk::raii::Device &&logicalDevice) -> void
{
  m_logicalDevice = std::move(logicalDevice);
}

auto Device::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
    -> std::expected<uint32_t, std::string>
{
  vk::PhysicalDeviceMemoryProperties memProperties = m_physicalDevice.getMemoryProperties();

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
  {
    if ((typeFilter & (1U << i)) != 0U &&
        (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
    {
      return i;
    }
  }

  return std::unexpected("Failed to find suitable memory type!");
}

auto Device::findSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling,
                                 vk::FormatFeatureFlags features)
    -> std::expected<vk::Format, std::string>
{
  for (const auto format : candidates)
  {
    vk::FormatProperties props = m_physicalDevice.getFormatProperties(format);

    if (((tiling == vk::ImageTiling::eLinear) &&
         ((props.linearTilingFeatures & features) == features)) ||
        ((tiling == vk::ImageTiling::eOptimal) &&
         ((props.optimalTilingFeatures & features) == features)))
    {
      return format;
    }
  }

  return std::unexpected("failed to find supported format!");
}

auto Device::findDepthFormat() -> std::expected<vk::Format, std::string>
{
  return findSupportedFormat(
      {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
      vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

} // namespace vksim
