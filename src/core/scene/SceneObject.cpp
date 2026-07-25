#include <utility>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "glm/ext/matrix_transform.hpp"
#include "glm/matrix.hpp"
#include "vksim/core/scene/SceneObject.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim
{

SceneObject::SceneObject(ResourceManager &resourceManager, VulkanContext &context)
    : m_resourceManager(resourceManager), m_context(context)
{
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

auto SceneObject::getAABB() const -> std::pair<glm::vec3, glm::vec3>
{
  // If the mesh is not set, return a default AABB (zero size).
  if (!hasResource<Mesh>())
  {
    spdlog::warn("Mesh is not set for scene object, returning default AABB");
    return {glm::vec3(0.0F), glm::vec3(0.0F)};
  }

  Mesh *mesh = *getResource<Mesh>();
  auto [min, max] = mesh->getAABB();

  // Generate Corners of the AABB in local space
  std::array<glm::vec3, 8> corners = {{
      {min.x, min.y, min.z},
      {max.x, min.y, min.z},
      {min.x, max.y, min.z},
      {max.x, max.y, min.z},
      {min.x, min.y, max.z},
      {max.x, min.y, max.z},
      {min.x, max.y, max.z},
      {max.x, max.y, max.z},
  }};

  // The model matrix is stored in a transposed form to match Vulkan's column-major, transpose
  // before transforming the AABB corners.
  auto modelMatrix = glm::transpose(getModelMatrix());

  glm::vec3 worldMin(FLT_MAX);
  glm::vec3 worldMax(-FLT_MAX);

  // Transform the corners to world space and compute the new AABB
  for (auto &corner : corners)
  {
    glm::vec3 world = glm::vec3(modelMatrix * glm::vec4(corner, 1.0f));

    worldMin = glm::min(worldMin, world);
    worldMax = glm::max(worldMax, world);
  }

  return {worldMin, worldMax};
}

} // namespace vksim
