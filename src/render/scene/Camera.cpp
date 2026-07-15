#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>

#include "vksim/render/scene/Camera.hpp"

namespace vksim
{

auto Camera::transform(const UpdateParams &updateParams) -> void
{
  if (updateParams.width.has_value())
  {
    m_width = *updateParams.width;
  }
  if (updateParams.height.has_value())
  {
    m_height = *updateParams.height;
  }
  if (updateParams.position.has_value())
  {
    params.cameraPos = *updateParams.position;
  }
  if (updateParams.center.has_value())
  {
    m_center = *updateParams.center;
  }
  if (updateParams.up.has_value())
  {
    m_up = *updateParams.up;
  }
  if (updateParams.fov.has_value())
  {
    m_fov = *updateParams.fov;
  }
  if (updateParams.nearPlane.has_value())
  {
    m_nearPlane = *updateParams.nearPlane;
  }
  if (updateParams.farPlane.has_value())
  {
    m_farPlane = *updateParams.farPlane;
  }

  const float safeHeight = (m_height == 0U) ? 1.0F : static_cast<float>(m_height);
  const float aspectRatio = static_cast<float>(m_width) / safeHeight;

  params.view = glm::lookAt(params.cameraPos, m_center, m_up);
  params.proj = glm::perspective(glm::radians(m_fov), aspectRatio, m_nearPlane, m_farPlane);
  params.proj[1][1] *= -1.0F;

  params.view = glm::transpose(params.view);
  params.proj = glm::transpose(params.proj);
}

} // namespace vksim
