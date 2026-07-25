#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <sys/types.h>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#include "vksim/core/resources/Mesh.hpp"
#include "vksim/core/resources/ResourceManager.hpp"

namespace vksim
{

/** @brief Represents a 3D object in the scene, encapsulating its mesh, textures, transform, and
 * visibility.
 */
struct Transform
{
  glm::vec3 position{0.0F, 0.0F, 0.0F};
  glm::quat rotation{1.0F, 0.0F, 0.0F, 0.0F}; // Quaternion for rotation
  glm::vec3 scale{1.0F, 1.0F, 1.0F};
};

/** @brief SceneObject class encapsulates the properties and behaviors of a 3D object in the scene,
 *        including its mesh, textures, transform, and visibility. It provides methods to set and
 * get resources, manage the object's transform, and compute its axis-aligned bounding box in world
 * space (AABB).
 * @note The SceneObject does not own the resources (mesh, texture, material) but holds references
 * to them through the ResourceManager. The ResourceManager is responsible for loading and managing
 * the lifetime of the resources. Resource refers to GPU resources like Mesh, Texture, Material etc.
 * Attributes like Fluid, RigidBody, etc. are not considered resources and are managed separately by
 * the SceneObject. The Scene is given to the physics engine, which then manages the physics
 * simulation for the scene objects based on their attributes.
 */
class SceneObject
{
public:
  SceneObject(ResourceManager &resourceManager, VulkanContext &context);

  /** @brief Sets a resource for the scene object using a unique identifier.
   * @tparam T Type of the resource (must be derived from Resource).
   * @param resourceId Unique identifier for the resource.
   */
  template <typename T>
    requires std::is_base_of_v<Resource, T>
  auto setResource(const std::string &resourceId) -> void
  {
    m_resourceIds[std::type_index(typeid(T))] = resourceId;
  }

  /** @brief Checks if a resource of the specified type is associated with the scene object.
   * @tparam T Type of the resource (must be derived from Resource).
   * @return True if the resource is associated, false otherwise.
   */
  template <typename T>
    requires std::is_base_of_v<Resource, T>
  [[nodiscard]] auto hasResource() const -> bool
  {
    return m_resourceIds.contains(std::type_index(typeid(T)));
  }

  /** @brief Gets the unique identifier for the resource of the specified type associated with the
   * scene object.
   * @tparam T Type of the resource (must be derived from Resource).
   * @return Reference to the unique identifier string.
   */
  template <typename T>
    requires std::is_base_of_v<Resource, T>
  [[nodiscard]] auto getResourceId() const -> std::expected<std::string, std::string>
  {
    auto iter = m_resourceIds.find(std::type_index(typeid(T)));
    if (iter == m_resourceIds.end())
    {
      return std::unexpected("Resource of type " + std::string(typeid(T).name()) +
                             " not found in scene object.");
    }
    return iter->second;
  }

  /** @brief Gets a resource for the scene object using a unique identifier.
   * @tparam T Type of the resource (must be derived from Resource).
   * @return Pointer to the resource if found, or an error message if not found.
   */
  template <typename T>
    requires std::is_base_of_v<Resource, T>
  [[nodiscard]] auto getResource() const -> std::expected<T *, std::string>
  {
    auto resourceId = getResourceId<T>();
    if (!resourceId)
    {
      return std::unexpected(resourceId.error());
    }
    return m_resourceManager.getResource<T>(*resourceId);
  }

  /** @brief Sets a component for the scene object using a type index.
   * @tparam T Type of the component (can be any type).
   */
  template <typename T> auto setPhysicsComponent() -> void
  {
    m_physicsComponents.insert(std::type_index(typeid(T)));
  }

  /** @brief Checks if a component of the specified type is associated with the scene object.
   * @tparam T Type of the component (can be any type).
   * @return True if the component is associated, false otherwise.
   */
  template <typename T> [[nodiscard]] auto hasPhysicsComponent() const -> bool
  {
    return m_physicsComponents.contains(std::type_index(typeid(T)));
  }

  /** @brief Sets the transform for the scene object.
   * @param transform Transform object containing position, rotation, and scale.
   */
  auto transform(const Transform &transform) -> void;
  [[nodiscard]] auto getTransform() const -> const Transform &;
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

  /** @brief Computes the axis-aligned bounding box (AABB) of the scene object based on its mesh and
   * transform.
   * @return A pair of glm::vec3 representing the minimum and maximum corners of the AABB.
   */
  [[nodiscard]]
  auto getAABB() const -> std::pair<glm::vec3, glm::vec3>;

private:
  // Reference to the resource manager for loading and accessing resources associated with the scene
  // object. The scene object does not own the resource manager; it is expected to be provided by
  // the scene.
  ResourceManager &m_resourceManager;

  // Reference to the Vulkan context for GPU resource management.
  VulkanContext &m_context;

  // Map to store resource identifiers for different types of resources (mesh, texture, material)
  // associated with the scene object.
  std::unordered_map<std::type_index, std::string> m_resourceIds;

  // Set to store physics components associated with the scene object. E.g. RigidBody, Softbody,
  // etc. The scene object can have multiple physics components, and this set allows for efficient
  // checking and retrieval of components by type.
  std::unordered_set<std::type_index> m_physicsComponents;

  // Transform object representing the position, rotation, and scale of the scene object in the
  // 3D space. Model matrix is derived from this transform and is used for rendering.
  Transform m_transform{};
  glm::mat4 m_modelMatrix = glm::mat4(1.0F);

  // Flag indicating whether the scene object is visible in the scene. If false, the object will not
  // be rendered.
  bool m_visible = true;

  // Unique identifier for the scene object, can be used for selection or identification
  uint32_t m_objectId{0};
};

} // namespace vksim
