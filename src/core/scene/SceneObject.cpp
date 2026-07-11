#include "glm/ext/matrix_transform.hpp"
#include <utility>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>

#include "vksim/core/scene/SceneObject.hpp"

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

  return m_resourceManager.GetResource<Mesh>(m_meshId);
}

auto SceneObject::setTexture(const std::string &textureId) -> void { m_textureId = textureId; }

auto SceneObject::getTextureId() const -> const std::string & { return m_textureId; }

auto SceneObject::getTexture() const -> std::expected<Texture *, std::string>
{
  return m_resourceManager.GetResource<Texture>(m_textureId);
}

auto SceneObject::setMaterial(const std::string &materialId) -> void { m_materialId = materialId; }

auto SceneObject::getMaterialId() const -> const std::string & { return m_materialId; }

auto SceneObject::getMaterial() const -> std::expected<Material *, std::string>
{
  return m_resourceManager.GetResource<Material>(m_materialId);
}

auto SceneObject::transform(const Transform &transform) -> void
{
  auto model = glm::mat4(1.0F);

  model = glm::translate(model, transform.position);
  model = glm::rotate(model, glm::radians(transform.rotation.x), glm::vec3(1.0F, 0.0F, 0.0F));
  model = glm::rotate(model, glm::radians(transform.rotation.y), glm::vec3(0.0F, 1.0F, 0.0F));
  model = glm::rotate(model, glm::radians(transform.rotation.z), glm::vec3(0.0F, 0.0F, 1.0F));

  model = glm::scale(model, transform.scale);

  // Transpose the model matrix to match Vulkan's column-major order
  model = glm::transpose(model);

  m_modelMatrix = model;
}

auto SceneObject::getModelMatrix() const -> const glm::mat4 & { return m_modelMatrix; }

auto SceneObject::setVisible(bool visible) -> void { m_visible = visible; }

auto SceneObject::isVisible() const -> bool { return m_visible; }

} // namespace vksim
