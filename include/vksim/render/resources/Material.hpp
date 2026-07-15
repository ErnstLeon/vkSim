#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/render/buffers/Image.hpp"
#include "vksim/render/resources/Resource.hpp"

namespace vksim
{

/** @brief Structure representing the properties of a material. */
struct MaterialInfo
{
  glm::vec3 m_baseColor = glm::vec3(1.0F, 1.0F, 1.0F);
  float m_metallic = 0.0F;
  float m_roughness = 0.0F;
};

/** @brief A class representing a material resource. This class encapsulates the properties of a
 * material and manages its uniform buffer on the GPU. Is is supposed to be loaded using the
 * ResourceManager and is used to bind material properties to shaders during rendering.
 */
class Material : public Resource
{
public:
  Material(Device &device, const std::string &identifier, const MaterialInfo &properties);

  [[nodiscard]] auto getBaseColor() const -> const glm::vec3 &;
  [[nodiscard]] auto getMetallic() const -> float;
  [[nodiscard]] auto getRoughness() const -> float;

  auto doLoad(UploadContext &uploadContext) -> bool override;

  /** @brief Returns the buffer associated with the material. */
  [[nodiscard]] auto getBuffer() const -> const Buffer & { return m_materialUniformBuffer; }

  /** @brief Returns the Vulkan buffer associated with the material. */
  [[nodiscard]] auto getVkBuffer() const -> const vk::raii::Buffer &
  {
    return m_materialUniformBuffer.getVkBuffer();
  }

private:
  MaterialInfo m_material;
  Buffer m_materialUniformBuffer;

  Device &m_device;
};
} // namespace vksim