#pragma once

#include <memory>
#include <vector>

#include "vksim/core/camera/Camera.hpp"
#include "vksim/core/context/VulkanContext.hpp"
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
  Scene() = default;
  explicit Scene(VulkanContext *context);

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
  [[nodiscard]] auto getObjects() const -> const std::vector<SceneObject> &;

  /** @brief Gets the list of scene objects in the scene.
   * @return Reference to the vector of scene objects.
   */
  [[nodiscard]] auto getObjects() -> std::vector<SceneObject> &;

private:
  Camera m_camera;
  std::vector<SceneObject> m_objects;

  VulkanContext *m_context = nullptr;
};

} // namespace vksim
