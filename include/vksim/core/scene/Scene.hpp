#pragma once

#include <vector>

#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/resources/ResourceManager.hpp"
#include "vksim/core/scene/Camera.hpp"
#include "vksim/core/scene/SceneObject.hpp"

namespace vksim
{

/**
 * @brief Scene class encapsulates the camera and scene objects, providing
 *        a high-level representation of a 3D scene for rendering.
 */
class Scene
{
public:
  Scene(VulkanContext &context, ResourceManager &resourceManager);

  /** @brief Adds a camera to the scene. If a camera already exists, it will be replaced.
   * @return Reference to the newly added camera.
   */
  auto addCamera() -> Camera &;

  /** @brief Gets the camera associated with the scene.
   * @return Reference to the camera.
   */
  [[nodiscard]] auto getCamera() -> Camera &;

  /** @brief Adds a new scene object to the scene.
   * @return Reference to the newly added scene object.
   */
  auto addObject() -> SceneObject &;

  /** @brief Clears all scene objects from the scene.
   */
  auto clearObjects() -> void;

  /** @brief Gets the list of scene objects in the scene.
   * @return Reference to the vector of scene objects.
   */
  [[nodiscard]] auto getObjects() const -> const std::vector<std::unique_ptr<SceneObject>> &;

  /** @brief Gets the list of scene objects in the scene.
   * @return Reference to the vector of scene objects.
   */
  [[nodiscard]] auto getObjects() -> std::vector<std::unique_ptr<SceneObject>> &;

private:
  // The scene owns the scene objects and camera, ensuring proper memory management.
  // Use std::unique_ptr for scene objects to be able to return stable references when adding new
  // objects.
  std::vector<std::unique_ptr<SceneObject>> m_objects;
  std::optional<Camera> m_camera;

  VulkanContext &m_context;
  ResourceManager &m_resourceManager;
};

} // namespace vksim
