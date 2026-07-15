#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <optional>

namespace
{
constexpr std::size_t LightAlignment = 16;
}

namespace vksim
{

/**
 * @struct DirectionalLight
 * @brief Represents a directional light source with direction and color properties.
 *
 * A directional light simulates light coming from an infinitely distant source
 * (like sunlight), affecting all objects uniformly from the same direction.
 */
struct DirectionalLight
{
  struct Params
  {
    glm::vec4 direction{0.0F, 0.0F, -1.0F, 0.0F}; ///< Light direction (xyz), unused (w)
    glm::vec4 color{1.0F, 1.0F, 1.0F, 1.0F};      ///< Light color (rgb), intensity (a)
  };

  // Parameters of the directional light, including direction, color, and intensity.
  Params params;

  /**
   * @struct UpdateParams
   * @brief Parameters for updating a directional light's properties.
   */
  struct UpdateParams
  {
    std::optional<glm::vec3> direction; ///< Optional new direction for the light
    std::optional<glm::vec3> color;     ///< Optional new color for the light
    std::optional<float> intensity;     ///< Optional new intensity for the light
  };

  /**
   * @brief Updates the directional light properties based on provided parameters.
   * @param params The update parameters containing optional new values
   */
  auto transform(const UpdateParams &params) -> void;

}; // namespace vksim

/**
 * @struct PointLight
 * @brief Represents a point light source that emits light in all directions.
 *
 * A point light simulates light emanating from a single point in space,
 * affecting objects based on their distance from the light source.
 */
struct PointLight
{
  struct Params
  {
    glm::vec4 position{0.0F, 0.0F, 1.0F, 0.0F}; ///< Light position (xyz), unused (w)
    glm::vec4 color{1.0F, 1.0F, 1.0F, 0.0F};    ///< Light color (rgb), intensity (a)
  };

  struct UpdateParams
  {
    std::optional<glm::vec3> position; ///< Optional new position for the light
    std::optional<glm::vec3> color;    ///< Optional new color for the light
    std::optional<float> intensity;    ///< Optional new intensity for the light
  };

  // Parameters of the point light, including position, color, and intensity.
  Params params;

  /**
   * @brief Updates the point light properties based on provided parameters.
   * @param params The update parameters containing optional new values
   */
  auto transform(const UpdateParams &params) -> void;
};

/**
 * @struct SpotLight
 * @brief Represents a spot light source that emits light in a cone-shaped direction.
 *
 * A spot light simulates light emanating from a single point in a specific direction
 * with a cone-shaped falloff, similar to a flashlight or stage light.
 */
struct SpotLight
{
  struct Params
  {
    glm::vec4 position{
        0.0F, 0.0F, 1.0F,
        0.75F}; ///< Light position (xyz), inner cone angle (w) as cosine of the angle
    glm::vec4 direction{
        0.0F, 0.0F, -1.0F,
        0.5F}; ///< Light direction (xyz), outer cone angle (w) as cosine of the angle
    glm::vec4 color{1.0F, 1.0F, 1.0F, 1.0F}; ///< Light color (rgb), intensity (a)
  };

  /**
   * @struct UpdateParams
   * @brief Parameters for updating a spot light's properties.
   */
  struct UpdateParams
  {
    std::optional<glm::vec3> position;  ///< Optional new position for the light
    std::optional<glm::vec3> direction; ///< Optional new direction for the light
    std::optional<glm::vec3> color;     ///< Optional new color for the light
    std::optional<float> intensity;     ///< Optional new intensity for the light
    std::optional<float> innerCone;     ///< Optional new inner cone angle
    std::optional<float> outerCone;     ///< Optional new outer cone angle
  };

  // Params of the spot light, including position, direction, color, and intensity.
  Params params;

  /**
   * @brief Updates the spot light properties based on provided parameters.
   * @param params The update parameters containing optional new values
   */
  auto transform(const UpdateParams &params) -> void;
};

} // namespace vksim