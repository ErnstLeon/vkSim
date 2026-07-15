#include <vector>

#include "vksim/render/context/VulkanContext.hpp"
#include "vksim/render/scene/Camera.hpp"
#include "vksim/render/scene/Scene.hpp"
#include "vksim/render/scene/SceneObject.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim
{

Scene::Scene(VulkanContext &context, ResourceManager &resourceManager)
    : m_context(context), m_resourceManager(resourceManager)
{
}

auto Scene::addCamera() -> Camera &
{
  m_camera.emplace();
  return *m_camera;
}

auto Scene::getCamera() -> Camera &
{
  if (!m_camera.has_value())
  {
    spdlog::error(
        "No camera has been added to the scene. Please add a camera before accessing it.");
    std::abort();
  }
  return *m_camera;
}

auto Scene::addObject() -> SceneObject &
{
  spdlog::info("Adding new scene object with id {}", m_objects.size());

  m_objects.emplace_back(std::make_unique<SceneObject>(m_resourceManager));
  m_objects.back()->setObjectId(static_cast<uint32_t>(m_objects.size() - 1));

  return *m_objects.back();
}

auto Scene::addDirectionalLight() -> DirectionalLight &
{
  spdlog::info("Adding new directional light with id {}", m_directionalLights.size());

  m_directionalLights.emplace_back(std::make_unique<DirectionalLight>());
  m_sceneInfo.numDirectionalLights += 1;
  return *m_directionalLights.back();
}

auto Scene::addPointLight() -> PointLight &
{
  spdlog::info("Adding new point light with id {}", m_pointLights.size());

  m_pointLights.emplace_back(std::make_unique<PointLight>());
  m_sceneInfo.numPointLights += 1;
  return *m_pointLights.back();
}

auto Scene::addSpotLight() -> SpotLight &
{
  spdlog::info("Adding new spot light with id {}", m_spotLights.size());

  m_spotLights.emplace_back(std::make_unique<SpotLight>());
  m_sceneInfo.numSpotLights += 1;
  return *m_spotLights.back();
}

auto Scene::getDirectionalLights() const -> const std::vector<std::unique_ptr<DirectionalLight>> &
{
  return m_directionalLights;
}

auto Scene::getPointLights() const -> const std::vector<std::unique_ptr<PointLight>> &
{
  return m_pointLights;
}

auto Scene::getSpotLights() const -> const std::vector<std::unique_ptr<SpotLight>> &
{
  return m_spotLights;
}

auto Scene::clearObjects() -> void { m_objects.clear(); }

auto Scene::clearDirectionalLights() -> void
{
  m_directionalLights.clear();
  m_sceneInfo.numDirectionalLights = 0;
}

auto Scene::clearPointLights() -> void
{
  m_pointLights.clear();
  m_sceneInfo.numPointLights = 0;
}

auto Scene::clearSpotLights() -> void
{
  m_spotLights.clear();
  m_sceneInfo.numSpotLights = 0;
}

auto Scene::getObjects() const -> const std::vector<std::unique_ptr<SceneObject>> &
{
  return m_objects;
}

auto Scene::getObjects() -> std::vector<std::unique_ptr<SceneObject>> & { return m_objects; }

auto Scene::getResourceManager() const -> ResourceManager & { return m_resourceManager; }

auto Scene::getSceneInfo() const -> const SceneInfo & { return m_sceneInfo; }

} // namespace vksim
