#include <memory>
#include <vector>

#include "vksim/core/camera/Camera.hpp"
#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/scene/Scene.hpp"
#include "vksim/core/scene/SceneObject.hpp"

namespace vksim
{

Scene::Scene(VulkanContext *context) : m_context(context) {}

auto Scene::addCamera() -> Camera &
{
  m_camera = Camera(m_context);
  return m_camera;
}

auto Scene::getCamera() -> Camera & { return m_camera; }

auto Scene::addObject() -> SceneObject &
{
  m_objects.emplace_back();
  return m_objects.back();
}

auto Scene::clearObjects() -> void { m_objects.clear(); }

auto Scene::getObjects() const -> const std::vector<SceneObject> & { return m_objects; }

} // namespace vksim
