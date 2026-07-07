#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>

#include "vksim/core/scene/Camera.hpp"

namespace vksim
{

auto Camera::transform(const UpdateParams &params) -> void
{
  if (params.width.has_value())
  {
    m_width = *params.width;
  }
  if (params.height.has_value())
  {
    m_height = *params.height;
  }
  if (params.position.has_value())
  {
    m_position = *params.position;
  }
  if (params.center.has_value())
  {
    m_center = *params.center;
  }
  if (params.up.has_value())
  {
    m_up = *params.up;
  }
  if (params.fov.has_value())
  {
    m_fov = *params.fov;
  }
  if (params.nearPlane.has_value())
  {
    m_nearPlane = *params.nearPlane;
  }
  if (params.farPlane.has_value())
  {
    m_farPlane = *params.farPlane;
  }

  const float safeHeight = (m_height == 0U) ? 1.0F : static_cast<float>(m_height);
  const float aspectRatio = static_cast<float>(m_width) / safeHeight;

  m_viewMatrix = glm::lookAt(m_position, m_center, m_up);
  m_projectionMatrix = glm::perspective(glm::radians(m_fov), aspectRatio, m_nearPlane, m_farPlane);
  m_projectionMatrix[1][1] *= -1.0F;

  m_viewMatrix = glm::transpose(m_viewMatrix);
  m_projectionMatrix = glm::transpose(m_projectionMatrix);
}

} // namespace vksim
