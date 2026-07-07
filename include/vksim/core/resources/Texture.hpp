#pragma once

#include <expected>
#include <string>
#include <typeindex>
#include <unordered_map>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <utility>
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/buffers/Image.hpp"
#include "vksim/core/resources/Resource.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim
{

/** @brief A class representing a texture resource. */
class Texture : public Resource
{
public:
  /** @brief Constructs a new texture resource.
   * @param device Reference to the Vulkan device for resource management.
   * @param identifier Unique identifier for the texture.
   * @param filePath Path to the texture image file.
   */
  explicit Texture(Device &device, const std::string &identifier, std::string filePath);

  Texture(const Texture &) = delete;
  Texture(Texture &&) noexcept = default;

  auto operator=(const Texture &) -> Texture & = delete;
  auto operator=(Texture &&) -> Texture & = delete;

  ~Texture() override = default;

  /** @brief Loads the texture resource.
   * @param uploadContext Upload command context that keeps staging buffers alive.
   * @return True if the texture was successfully loaded, false otherwise.
   */
  auto doLoad(UploadContext &uploadContext) -> bool override;

  /** @brief Returns the underlying Vulkan image.
   * @return Reference to the Vulkan image.
   */
  [[nodiscard]] auto getImage() const -> const Image & { return m_image; }

  /** @brief Returns the image view for the texture.
   * @return Reference to the Vulkan image view.
   */
  [[nodiscard]] auto getImageView() const -> const vk::raii::ImageView & { return m_imageView; }

  /** @brief Returns the sampler for the texture.
   * @return Reference to the Vulkan sampler.
   */
  [[nodiscard]] auto getSampler() const -> const vk::raii::Sampler & { return m_sampler; }

private:
  /** @brief Loads the texture from a file and creates the necessary Vulkan resources.
   * @param uploadContext Upload command context that keeps staging buffers alive.
   */
  auto loadFromFile(UploadContext &uploadContext) -> void;

  /** @brief Creates an image view for the texture. */
  auto createImageView() -> void;

  /** @brief Creates a sampler for the texture. */
  auto createSampler() -> void;

  // Core Vulkan GPU resources for texture representation
  Image m_image;                             // Vulkan image object representing the texture
  vk::raii::ImageView m_imageView = nullptr; // Shader-accessible view into the image
  vk::raii::Sampler m_sampler = nullptr;     // Sampling configuration (filtering, wrapping, etc.)

  std::string m_filePath; // Path to the texture image file
  Device &m_device;       // Reference to the Vulkan device for resource management
};
} // namespace vksim