#include <utility>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>

#include "vksim/core/scene/SceneObject.hpp"

namespace vksim
{

auto Transform::modelMatrix() const -> glm::mat4
{
  glm::mat4 model(1.0F);
  model = glm::translate(model, position);
  model = glm::rotate(model, rotation.x, glm::vec3(1.0F, 0.0F, 0.0F));
  model = glm::rotate(model, rotation.y, glm::vec3(0.0F, 1.0F, 0.0F));
  model = glm::rotate(model, rotation.z, glm::vec3(0.0F, 0.0F, 1.0F));
  model = glm::scale(model, scale);
  return model;
}

SceneObject::SceneObject(std::shared_ptr<Mesh> mesh) : m_mesh(std::move(mesh)) {}

auto SceneObject::setMesh(std::shared_ptr<Mesh> mesh) -> void { m_mesh = std::move(mesh); }

auto SceneObject::getMesh() const -> const std::shared_ptr<Mesh> & { return m_mesh; }

auto SceneObject::setTextures(std::vector<std::shared_ptr<Texture>> textures) -> void
{
  m_textures = std::move(textures);
}

auto SceneObject::addTexture(std::shared_ptr<Texture> texture) -> void
{
  m_textures.push_back(std::move(texture));
}

auto SceneObject::getTextures() const -> const std::vector<std::shared_ptr<Texture>> &
{
  return m_textures;
}

auto SceneObject::setTransform(const Transform &transform) -> void { m_transform = transform; }

auto SceneObject::getTransform() const -> const Transform & { return m_transform; }

auto SceneObject::getTransform() -> Transform & { return m_transform; }

auto SceneObject::setVisible(bool visible) -> void { m_visible = visible; }

auto SceneObject::isVisible() const -> bool { return m_visible; }

} // namespace vksim
