#pragma once

#include <expected>
#include <string>
#include <sys/types.h>
#include <vector>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#include "vksim/core/resources/Material.hpp"
#include "vksim/core/resources/Mesh.hpp"
#include "vksim/core/resources/ResourceManager.hpp"
#include "vksim/core/resources/Texture.hpp"

namespace vksim
{

/** @brief Represents a 3D object in the scene, encapsulating its mesh, textures, transform, and
 * visibility.
 */
struct Transform
{
  glm::vec3 position{0.0F, 0.0F, 0.0F};
  glm::vec3 rotation{0.0F, 0.0F, 0.0F};
  glm::vec3 scale{1.0F, 1.0F, 1.0F};
};

/** @brief SceneObject class encapsulates the properties and behaviors of a 3D object in the scene,
 *        including its mesh, textures, transform, and visibility.
 */
class SceneObject
{
public:
  explicit SceneObject(ResourceManager &resourceManager);

  /** @brief Sets the mesh for the scene object using a unique identifier.
   * @param meshId Unique identifier for the mesh resource.
   */
  auto setMesh(const std::string &meshId) -> void;
  [[nodiscard]] auto getMeshId() const -> const std::string &;
  [[nodiscard]] auto getMesh() const -> std::expected<Mesh *, std::string>;

  /** @brief Sets the texture for the scene object using a unique identifier.
   * @param textureId Unique identifier for the texture resource.
   */
  auto setTexture(const std::string &textureId) -> void;
  [[nodiscard]] auto getTextureId() const -> const std::string &;
  [[nodiscard]] auto getTexture() const -> std::expected<Texture *, std::string>;

  /** @brief Sets the material for the scene object using a unique identifier.
   * @param materialId Unique identifier for the material resource.
   */
  auto setMaterial(const std::string &materialId) -> void;
  [[nodiscard]] auto getMaterialId() const -> const std::string &;
  [[nodiscard]] auto getMaterial() const -> std::expected<Material *, std::string>;

  /** @brief Sets the transform for the scene object.
   * @param transform Transform object containing position, rotation, and scale.
   */
  auto transform(const Transform &transform) -> void;
  [[nodiscard]] auto getModelMatrix() const -> const glm::mat4 &;

  /** @brief Sets the visibility of the scene object.
   * @param visible True to make the object visible, false to hide it.
   */
  auto setVisible(bool visible) -> void;
  [[nodiscard]] auto isVisible() const -> bool;

  /** @brief Sets a unique identifier for the scene object, which can be used for selection or
   * identification.
   * @param objectId Unique identifier for the scene object.
   */
  auto setObjectId(uint32_t objectId) -> void { m_objectId = objectId; }

  /** @brief Gets the unique identifier for the scene object.
   * @return Unique identifier for the scene object.
   */
  [[nodiscard]] auto getObjectId() const -> uint32_t { return m_objectId; }

private:
  glm::mat4 m_modelMatrix = glm::mat4(1.0F);
  bool m_visible = true;

  ResourceManager &m_resourceManager;
  std::string m_meshId;
  std::string m_textureId;
  std::string m_materialId;

  // Unique identifier for the scene object, can be used for selection or identification
  uint32_t m_objectId{0};
};

} // namespace vksim
