#include "glm/ext/matrix_transform.hpp"
#include "glm/matrix.hpp"
#include <utility>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "vksim/render/scene/SceneObject.hpp"

namespace vksim
{

SceneObject::SceneObject(ResourceManager &resourceManager) : m_resourceManager(resourceManager) {}

auto SceneObject::setMesh(const std::string &meshId) -> void { m_meshId = meshId; }

auto SceneObject::getMeshId() const -> const std::string & { return m_meshId; }

auto SceneObject::getMesh() const -> std::expected<Mesh *, std::string>
{

  if (m_meshId.empty())
  {
    return std::unexpected("Mesh id is not set for scene object");
  }

  return m_resourceManager.getResource<Mesh>(m_meshId);
}

auto SceneObject::setTexture(const std::string &textureId) -> void { m_textureId = textureId; }

auto SceneObject::getTextureId() const -> const std::string & { return m_textureId; }

auto SceneObject::getTexture() const -> std::expected<Texture *, std::string>
{
  return m_resourceManager.getResource<Texture>(m_textureId);
}

auto SceneObject::setMaterial(const std::string &materialId) -> void { m_materialId = materialId; }

auto SceneObject::getMaterialId() const -> const std::string & { return m_materialId; }

auto SceneObject::getMaterial() const -> std::expected<Material *, std::string>
{
  return m_resourceManager.getResource<Material>(m_materialId);
}

auto SceneObject::transform(const Transform &transform) -> void
{
  m_transform = transform;

  // Build the transformation matrix from the position, rotation, and scale of the object. The
  // transformation matrix is constructed in the order of translation, rotation (X, Y, Z), and
  // scale.
  glm::mat4 rotation = glm::mat4_cast(transform.rotation);
  glm::mat4 model = glm::translate(glm::mat4(1.0F), transform.position) * rotation *
                    glm::scale(glm::mat4(1.0F), transform.scale);

  // Transpose the model matrix to match Vulkan's column-major order.
  m_modelMatrix = glm::transpose(model);
}

auto SceneObject::getTransform() const -> const Transform & { return m_transform; }

auto SceneObject::getModelMatrix() const -> const glm::mat4 & { return m_modelMatrix; }

auto SceneObject::setVisible(bool visible) -> void { m_visible = visible; }

auto SceneObject::isVisible() const -> bool { return m_visible; }

} // namespace vksim
