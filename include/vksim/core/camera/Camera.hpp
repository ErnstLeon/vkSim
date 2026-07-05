#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <optional>

#include "vksim/core/buffers/Buffer.hpp"
#include "vksim/core/context/VulkanContext.hpp"

static constexpr float fov = 45.0F;
static constexpr float nearPlane = 0.1F;
static constexpr float farPlane = 100.0F;

constexpr std::size_t uniformAlignment = 16;
namespace vksim
{

/** @brief Structure to hold the camera's uniform buffer object data, including view and projection
 * matrices. */
struct CameraUniformBufferObject
{
  alignas(uniformAlignment) glm::mat4 view;
  alignas(uniformAlignment) glm::mat4 proj;
};

/** @brief Camera class encapsulates the camera's position, orientation, and projection parameters,
 *        and manages the associated uniform buffer for rendering. */
class Camera
{
public:
  struct UpdateParams
  {
    std::optional<uint32_t> width;
    std::optional<uint32_t> height;
    std::optional<glm::vec3> position;
    std::optional<glm::vec3> center;
    std::optional<glm::vec3> up;
    std::optional<float> fov;
    std::optional<float> nearPlane;
    std::optional<float> farPlane;
  };

  Camera() = default;
  Camera(const Camera &) = delete;
  Camera(Camera &&) noexcept = default;

  auto operator=(const Camera &) -> Camera & = delete;
  auto operator=(Camera &&) -> Camera & = default;

  Camera(VulkanContext *context, uint32_t framesInFlight = 1);

  /** @brief Updates only the provided camera parameters.
   * Example: camera.update({.center = glm::vec3(0.0F, 0.0F, 0.0F)});
   */
  auto update(const UpdateParams &params, uint32_t frameIndex = 0) -> void;

  /** @brief Returns the uniform buffer associated with the camera.
   * @return Reference to the Vulkan buffer containing the camera's uniform data.
   */
  [[nodiscard]] auto getUniformBuffer(uint32_t frameIndex = 0) const -> const vk::raii::Buffer &
  {
    return m_uniformBuffers[frameIndex].getVkBuffer();
  }

  /** @brief Returns the camera's position in world space.
   * @return The camera's position vector.
   */
  [[nodiscard]] auto getPosition() const -> const glm::vec3 & { return m_position; }
  /** @brief Returns the point the camera is looking at in world space.
   * @return The camera's center vector.
   */
  [[nodiscard]] auto getCenter() const -> const glm::vec3 & { return m_center; }
  /** @brief Returns the up direction vector for the camera.
   * @return The camera's up vector.
   */
  [[nodiscard]] auto getUp() const -> const glm::vec3 & { return m_up; }
  /** @brief Returns the width of the viewport.
   * @return The viewport width.
   */
  [[nodiscard]] auto getWidth() const -> uint32_t { return m_width; }
  /** @brief Returns the height of the viewport.
   * @return The viewport height.
   */
  [[nodiscard]] auto getHeight() const -> uint32_t { return m_height; }
  /** @brief Returns the field of view of the camera.
   * @return The camera's field of view in degrees.
   */
  [[nodiscard]] auto getFov() const -> float { return m_fov; }
  /** @brief Returns the near plane distance of the camera.
   * @return The camera's near plane distance.
   */
  [[nodiscard]] auto getNearPlane() const -> float { return m_nearPlane; }
  /** @brief Returns the far plane distance of the camera.
   * @return The camera's far plane distance.
   */
  [[nodiscard]] auto getFarPlane() const -> float { return m_farPlane; }

  /** @brief Returns the view matrix of the camera.
   * @return The camera's view matrix.
   */
  [[nodiscard]] auto getViewMatrix() const -> const glm::mat4 & { return m_viewMatrix; }
  /** @brief Returns the projection matrix of the camera.
   * @return The camera's projection matrix.
   */
  [[nodiscard]] auto getProjectionMatrix() const -> const glm::mat4 & { return m_projectionMatrix; }

private:
  VulkanContext *m_context = nullptr;
  std::vector<Buffer> m_uniformBuffers;
  std::vector<void *> m_uniformBuffersMapped;

  uint32_t m_framesInFlight = 1;

  glm::mat4 m_viewMatrix = glm::mat4(1.0F);
  glm::mat4 m_projectionMatrix = glm::mat4(1.0F);
  uint32_t m_width = 0;
  uint32_t m_height = 0;

  glm::vec3 m_position = glm::vec3(0.0F, 0.0F, 1.0F);
  glm::vec3 m_center = glm::vec3(0.0F, 0.0F, 0.0F);
  glm::vec3 m_up = glm::vec3(0.0F, 1.0F, 0.0F);

  float m_fov{fov};
  float m_nearPlane{nearPlane};
  float m_farPlane{farPlane};
};
} // namespace vksim