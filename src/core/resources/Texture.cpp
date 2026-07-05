#include <expected>
#include <string>
#include <typeindex>
#include <unordered_map>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <utility>
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/buffers/Buffer.hpp"
#include "vksim/core/buffers/Image.hpp"
#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/resources/Resource.hpp"
#include "vksim/core/resources/Texture.hpp"

namespace vksim
{

Texture::Texture(const std::string &identifier, std::string filePath, VulkanContext *context)
    : Resource(identifier), m_filePath(std::move(filePath)), m_context(context)
{
}

auto Texture::doLoad(UploadContext &uploadContext) -> bool
{
  loadFromFile(uploadContext);
  createImageView();
  createSampler();

  spdlog::info("Texture {} loaded successfully", GetId());

  return true;
}

auto Texture::loadFromFile(UploadContext &uploadContext) -> void
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

  // Create a staging buffer to hold the pixel data
  auto stagingBuffer =
      Buffer(m_context, BufferCreateInfo{.size = imageSize,
                                         .usage = vk::BufferUsageFlagBits::eTransferSrc,
                                         .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                                       vk::MemoryPropertyFlagBits::eHostCoherent});

  auto *stagingBufferMemory = stagingBuffer.getVkBufferMemory().mapMemory(0, imageSize);
  std::memcpy(stagingBufferMemory, pixels, static_cast<size_t>(imageSize));
  stagingBuffer.getVkBufferMemory().unmapMemory();
  stbi_image_free(pixels);

  // Create the Vulkan image with the appropriate properties
  m_image =
      Image(m_context, ImageCreateInfo{.width = static_cast<uint32_t>(texWidth),
                                       .height = static_cast<uint32_t>(texHeight),
                                       .numSamples = vk::SampleCountFlagBits::e1,
                                       .format = vk::Format::eR8G8B8A8Srgb,
                                       .tiling = vk::ImageTiling::eOptimal,
                                       .usage = vk::ImageUsageFlagBits::eTransferDst |
                                                vk::ImageUsageFlagBits::eSampled,
                                       .properties = vk::MemoryPropertyFlagBits::eDeviceLocal});

  // Transition the image layout to be optimal for transfer destination
  m_image.transitionLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, {},
                           vk::AccessFlagBits2::eTransferWrite,
                           vk::PipelineStageFlagBits2::eTopOfPipe,
                           vk::PipelineStageFlagBits2::eTransfer, vk::ImageAspectFlagBits::eColor,
                           uploadContext.getCommandBuffer());

  // Copy the pixel data from the staging buffer to the Vulkan image
  m_image.copyFromBuffer(stagingBuffer, static_cast<uint32_t>(texWidth),
                         static_cast<uint32_t>(texHeight), uploadContext.getCommandBuffer());

  // Transition the image layout to be optimal for shader read access
  m_image.transitionLayout(
      vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
      vk::AccessFlagBits2::eTransferWrite, vk::AccessFlagBits2::eShaderRead,
      vk::PipelineStageFlagBits2::eTransfer, vk::PipelineStageFlagBits2::eFragmentShader,
      vk::ImageAspectFlagBits::eColor, uploadContext.getCommandBuffer());

  // Keep the staging buffer alive until the upload is complete
  uploadContext.addStagingBuffer(std::move(stagingBuffer));
}

auto Texture::createImageView() -> void
{
  m_imageView = m_image.getVkImageView(ImageViewCreateInfo{
      .format = vk::Format::eR8G8B8A8Srgb, .aspectFlags = vk::ImageAspectFlagBits::eColor});
}

auto Texture::createSampler() -> void
{
  vk::PhysicalDeviceProperties properties = m_context->getPhysicalDevice().getProperties();
  vk::SamplerCreateInfo samplerInfo{.magFilter = vk::Filter::eLinear,
                                    .minFilter = vk::Filter::eLinear,
                                    .mipmapMode = vk::SamplerMipmapMode::eLinear,
                                    .addressModeU = vk::SamplerAddressMode::eRepeat,
                                    .addressModeV = vk::SamplerAddressMode::eRepeat,
                                    .addressModeW = vk::SamplerAddressMode::eRepeat,
                                    .anisotropyEnable = vk::True,
                                    .maxAnisotropy = properties.limits.maxSamplerAnisotropy};

  m_sampler = vk::raii::Sampler(m_context->getDevice(), samplerInfo);
}

auto Texture::doUnload() -> bool
{
  m_image = {};
  m_imageView = nullptr;
  m_sampler = nullptr;
  return true;
}
} // namespace vksim