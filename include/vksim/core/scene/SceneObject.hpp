#pragma once

#include <memory>
#include <vector>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#include "vksim/core/resources/Mesh.hpp"
#include "vksim/core/resources/Resource.hpp"
#include "vksim/core/resources/Texture.hpp"

namespace vksim
{

struct Transform
{
  glm::vec3 position{0.0F, 0.0F, 0.0F};
  glm::vec3 rotation{0.0F, 0.0F, 0.0F};
  glm::vec3 scale{1.0F, 1.0F, 1.0F};

  [[nodiscard]] auto modelMatrix() const -> glm::mat4;
};

class SceneObject
{
public:
  SceneObject() = default;
  explicit SceneObject(std::shared_ptr<Mesh> mesh);

  template <typename T>
    requires std::is_base_of_v<Resource, T>
  auto addResource(T *resource) -> void
  {
    m_resources.push_back(resource);
  }

  [[nodiscard]] auto getTransform() const -> const Transform & { return m_transform; }

  auto setVisible(bool visible) -> void;
  [[nodiscard]] auto isVisible() const -> bool;

private:
  Transform m_transform{};
  bool m_visible = true;

  std::vector<Resource> m_resources;
};

} // namespace vksim
