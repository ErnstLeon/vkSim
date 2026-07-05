#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>

#include "vksim/core/camera/Camera.hpp"

namespace vksim
{

Camera::Camera(VulkanContext *context, uint32_t framesInFlight)
    : m_context(context), m_framesInFlight(framesInFlight)
{
  if (m_context == nullptr)
  {
    spdlog::error("Camera context is null!");
    std::abort();
  }

  m_uniformBuffers.resize(m_framesInFlight);
  m_uniformBuffersMapped.resize(m_framesInFlight);
  for (uint32_t i = 0; i < m_framesInFlight; ++i)
  {
    m_uniformBuffers[i] = Buffer(
        m_context, BufferCreateInfo{.size = sizeof(CameraUniformBufferObject),
                                    .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                                    .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                                  vk::MemoryPropertyFlagBits::eHostCoherent});
    m_uniformBuffersMapped[i] =
        m_uniformBuffers[i].getVkBufferMemory().mapMemory(0, sizeof(CameraUniformBufferObject));
  }

  spdlog::info("Camera initialized with {} uniform buffers for {} frames in flight",
               m_uniformBuffers.size(), m_framesInFlight);
}

auto Camera::update(const UpdateParams &params, uint32_t frameIndex) -> void
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

  m_viewMatrix = lookAt(m_position, m_center, m_up);
  m_projectionMatrix = glm::perspective(glm::radians(m_fov), aspectRatio, m_nearPlane, m_farPlane);
  m_projectionMatrix[1][1] *= -1.0F;

  if (m_context == nullptr)
  {
    spdlog::error("Camera context is null!");
    std::abort();
  }

  m_viewMatrix = glm::transpose(m_viewMatrix);
  m_projectionMatrix = glm::transpose(m_projectionMatrix);

  CameraUniformBufferObject ubo{.view = m_viewMatrix, .proj = m_projectionMatrix};
  void *mapped = m_uniformBuffersMapped[frameIndex];
  std::memcpy(mapped, &ubo, sizeof(CameraUniformBufferObject));
}

} // namespace vksim
