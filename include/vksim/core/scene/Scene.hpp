#pragma once

#include <vector>

#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/physics/LBMFluid.hpp"
#include "vksim/core/resources/ResourceManager.hpp"
#include "vksim/core/scene/Camera.hpp"
#include "vksim/core/scene/Light.hpp"
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
  /** @brief Structure representing the information of a scene, including the number of lights. */
  struct SceneInfo
  {
    uint32_t numDirectionalLights{0};
    uint32_t numPointLights{0};
    uint32_t numSpotLights{0};
  };

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

  /** @brief Adds a new directional light to the scene.
   * @return Reference to the newly added directional light.
   */
  auto addDirectionalLight() -> DirectionalLight &;

  /** @brief Adds a new point light to the scene.
   * @return Reference to the newly added point light.
   */
  auto addPointLight() -> PointLight &;

  /** @brief Adds a new spot light to the scene.
   * @return Reference to the newly added spot light.
   */
  auto addSpotLight() -> SpotLight &;

  /** @brief Gets the list of directional lights in the scene.
   * @return Reference to the vector of directional lights.
   */
  [[nodiscard]]
  auto getDirectionalLights() const -> const std::vector<std::unique_ptr<DirectionalLight>> &;

  /** @brief Gets the list of point lights in the scene.
   * @return Reference to the vector of point lights.
   */
  [[nodiscard]]
  auto getPointLights() const -> const std::vector<std::unique_ptr<PointLight>> &;

  /** @brief Gets the list of spot lights in the scene.
   * @return Reference to the vector of spot lights.
   */
  [[nodiscard]]
  auto getSpotLights() const -> const std::vector<std::unique_ptr<SpotLight>> &;

  /** @brief Clears all directional lights from the scene.
   */
  auto clearDirectionalLights() -> void;

  /** @brief Clears all point lights from the scene.
   */
  auto clearPointLights() -> void;

  /** @brief Clears all spot lights from the scene.
   */
  auto clearSpotLights() -> void;

  /** @brief Clears all scene objects from the scene.
   */
  auto clearObjects() -> void;

  /** @brief Gets the scene information, including the number of lights in the scene.
   * @return Reference to the SceneInfo structure.
   */
  [[nodiscard]]
  auto getSceneInfo() const -> const SceneInfo &;

  /** @brief Gets the list of scene objects in the scene.
   * @return Reference to the vector of scene objects.
   */
  [[nodiscard]] auto getObjects() const -> const std::vector<std::unique_ptr<SceneObject>> &;

  /** @brief Gets the list of scene objects in the scene.
   * @return Reference to the vector of scene objects.
   */
  [[nodiscard]] auto getObjects() -> std::vector<std::unique_ptr<SceneObject>> &;

  /** @brief Gets the resource manager associated with the scene.
   * @return Reference to the resource manager.
   */
  [[nodiscard]] auto getResourceManager() const -> ResourceManager &;

  /** @brief Computes the axis-aligned bounding box (AABB) of the scene based on its objects.
   * @return A pair of glm::vec3 representing the minimum and maximum corners of the AABB.
   */
  [[nodiscard]]
  auto getAABB() const -> std::pair<glm::vec3, glm::vec3>;

private:
  // The scene owns the scene objects and camera, ensuring proper memory management.
  // Use std::unique_ptr for scene objects to be able to return stable references when adding new
  // objects. Same for light objects.
  std::vector<std::unique_ptr<SceneObject>> m_objects;
  std::vector<std::unique_ptr<PointLight>> m_pointLights;
  std::vector<std::unique_ptr<DirectionalLight>> m_directionalLights;
  std::vector<std::unique_ptr<SpotLight>> m_spotLights;
  std::optional<Camera> m_camera;

  // The scene own resources for physical simulation, such as LBM fluid simulation, and manages
  // their lifetimes.
  std::vector<std::unique_ptr<physics::LBMFluidBase>> m_lbmFluids;

  VulkanContext &m_context;
  ResourceManager &m_resourceManager;
  SceneInfo m_sceneInfo;
};

} // namespace vksim
