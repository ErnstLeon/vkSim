#include "vksim/core/scene/Light.hpp"
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>

namespace vksim
{

auto DirectionalLight::transform(const UpdateParams &updateParams) -> void
{
  if (updateParams.direction)
  {
    // Normalize the direction and set it in the xyz components
    params.direction = glm::vec4(glm::normalize(updateParams.direction.value()), 0.0F);
  }

  if (updateParams.color)
  {
    // Set the new color in the rgb components, preserve intensity in w
    params.color = glm::vec4(updateParams.color.value(), params.color.w);
  }

  if (updateParams.intensity)
  {
    // Set the intensity in the w component of direction
    params.color.w = updateParams.intensity.value();
  }
}

auto PointLight::transform(const UpdateParams &updateParams) -> void
{
  if (updateParams.position)
  {
    // Set the new position in the xyz components
    params.position = glm::vec4(updateParams.position.value(), 0.0F);
  }

  if (updateParams.color)
  {
    // Set the new color in the rgb components, preserve intensity in w
    params.color = glm::vec4(updateParams.color.value(), params.color.w);
  }

  if (updateParams.intensity)
  {
    // Set the intensity in the w component of color
    params.color.w = updateParams.intensity.value();
  }
}

auto SpotLight::transform(const UpdateParams &updateParams) -> void
{
  if (updateParams.position)
  {
    // Set the new position in the xyz components, preserve inner cone angle in w
    params.position = glm::vec4(updateParams.position.value(), params.position.w);
  }

  if (updateParams.direction)
  {
    // Set the normalized direction in the xyz components, preserve outer cone angle in w
    auto normalizedDir = glm::normalize(updateParams.direction.value());
    params.direction = glm::vec4(normalizedDir, params.direction.w);
  }

  if (updateParams.color)
  {
    // Set the new color in the rgb components, preserve intensity in w
    params.color = glm::vec4(updateParams.color.value(), params.color.w);
  }

  if (updateParams.intensity)
  {
    // Set the intensity
    params.color.w = updateParams.intensity.value();
  }

  if (updateParams.innerCone)
  {
    // Set the inner cone angle in the w component of position
    params.position.w = glm::cos(updateParams.innerCone.value());
    spdlog::info("SpotLight inner cone angle set to {} degrees (cosine: {})",
                 glm::degrees(updateParams.innerCone.value()), params.position.w);
  }

  if (updateParams.outerCone)
  {
    // Set the outer cone angle in the w component of direction
    params.direction.w = glm::cos(updateParams.outerCone.value());
    spdlog::info("SpotLight outer cone angle set to {} degrees (cosine: {})",
                 glm::degrees(updateParams.outerCone.value()), params.direction.w);
  }
}

} // namespace vksim
