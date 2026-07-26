#include "vksim/core/context/VulkanContext.hpp"
#include <string>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <utility>
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/buffers/Buffer.hpp"
#include "vksim/core/buffers/Image.hpp"
#include "vksim/core/resources/Resource.hpp"
#include "vksim/core/resources/Texture.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim
{

Texture::Texture(VulkanContext &context, const std::string &identifier, std::string filePath)
    : Resource(identifier), m_filePath(std::move(filePath)), m_context(context), m_image(context),
      m_imageView(nullptr), m_sampler(nullptr)
{
}

auto Texture::doLoad() -> bool
{
  loadFromFile();
  createImageView();
  createSampler();

  spdlog::info("Texture {} loaded successfully", GetId());

  return true;
}

auto Texture::loadFromFile() -> void
{
  // Load the image using stb_image
  int texWidth;
  int texHeight;
  int texChannels;
  stbi_uc *pixels =
      stbi_load(m_filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
  vk::DeviceSize imageSize = texWidth * texHeight * 4;

  if (pixels == nullptr)
  {
    spdlog::error("failed to load texture image");
    std::abort();
  }

  // Create the Vulkan image with the appropriate properties
  m_image.create(ImageCreateInfo{.width = static_cast<uint32_t>(texWidth),
                                 .height = static_cast<uint32_t>(texHeight),
                                 .numSamples = vk::SampleCountFlagBits::e1,
                                 .format = vk::Format::eR8G8B8A8Srgb,
                                 .tiling = vk::ImageTiling::eOptimal,
                                 .usage = vk::ImageUsageFlagBits::eTransferDst |
                                          vk::ImageUsageFlagBits::eSampled,
                                 .properties = vk::MemoryPropertyFlagBits::eDeviceLocal});

  // Copy the pixel data from the host to the Vulkan image
  m_image.copyFromHost(pixels, static_cast<uint32_t>(imageSize));
  stbi_image_free(pixels);
}

auto Texture::createImageView() -> void
{
  m_imageView = m_image.getVkImageView(ImageViewCreateInfo{
      .format = vk::Format::eR8G8B8A8Srgb, .aspectFlags = vk::ImageAspectFlagBits::eColor});
}

auto Texture::createSampler() -> void
{
  vk::PhysicalDeviceProperties properties = m_context.getDevice().physical().getProperties();
  vk::SamplerCreateInfo samplerInfo{.magFilter = vk::Filter::eLinear,
                                    .minFilter = vk::Filter::eLinear,
                                    .mipmapMode = vk::SamplerMipmapMode::eLinear,
                                    .addressModeU = vk::SamplerAddressMode::eRepeat,
                                    .addressModeV = vk::SamplerAddressMode::eRepeat,
                                    .addressModeW = vk::SamplerAddressMode::eRepeat,
                                    .anisotropyEnable = vk::True,
                                    .maxAnisotropy = properties.limits.maxSamplerAnisotropy};

  m_sampler = vk::raii::Sampler(m_context.getDevice().logical(), samplerInfo);
}

} // namespace vksim