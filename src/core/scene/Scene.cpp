#include <vector>

#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/scene/Camera.hpp"
#include "vksim/core/scene/Scene.hpp"
#include "vksim/core/scene/SceneObject.hpp"

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

auto Scene::clearObjects() -> void { m_objects.clear(); }

auto Scene::getObjects() const -> const std::vector<std::unique_ptr<SceneObject>> &
{
  return m_objects;
}

auto Scene::getObjects() -> std::vector<std::unique_ptr<SceneObject>> & { return m_objects; }

} // namespace vksim
