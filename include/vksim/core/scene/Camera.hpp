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

namespace
{
constexpr float fov = 45.0F;
constexpr float nearPlane = 0.1F;
constexpr float farPlane = 100.0F;

constexpr std::size_t CameraAlignment = 16;

} // namespace

namespace vksim
{

/** @brief Camera struct encapsulates the camera's position, orientation, and projection parameters,
 *        and manages the associated uniform buffer for rendering. */
struct Camera
{
public:
  struct Params
  {

    alignas(CameraAlignment) glm::mat4 view{glm::mat4(1.0F)};
    alignas(CameraAlignment) glm::mat4 proj{glm::mat4(1.0F)};
    alignas(CameraAlignment) glm::vec3 cameraPos{glm::vec3(0.0F, 0.0F, 0.0F)};
  };

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

  // Parameters of the camera, including view and projection matrices, and camera position.
  Params params;

  /** @brief Updates the camera's parameters and recalculates the view and projection matrices.
   * @param params Structure containing the parameters to update. Only the provided parameters will
   * be updated; others will remain unchanged.
   * @note The view and projection matrices are recalculated based on the updated parameters. The
   * matrices are transposed to match Vulkan's column-major order. Example:
   * camera.transform({.center = glm::vec3(0.0F, 0.0F, 0.0F)});
   */
  auto transform(const UpdateParams &params) -> void;

  uint32_t m_width = 0;
  uint32_t m_height = 0;

  glm::vec3 m_center = glm::vec3(0.0F, 0.0F, 0.0F);
  glm::vec3 m_up = glm::vec3(0.0F, 1.0F, 0.0F);

  float m_fov{fov};
  float m_nearPlane{nearPlane};
  float m_farPlane{farPlane};
};
} // namespace vksim