
#include "glm/trigonometric.hpp"
#include "vksim/render/scene/SceneObject.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/hash.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <string>
#include <vulkan/vulkan_raii.hpp>

#include <ImGuizmo.h>

#include "vksim/imgui/ImGuiUtil.hpp"
#include "vksim/imgui/fonts/IconsFontAwesome7.h"
#include "vksim/render/buffers/Buffer.hpp"
#include "vksim/render/buffers/Image.hpp"
#include "vksim/render/device/Device.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim::ImGui
{

namespace
{

//////////////////////////////////////////////////
// Constants for ImGui and Gizmo Utilities
//////////////////////////////////////////////////

constexpr float kPositionDragSpeed = 0.05F;
constexpr float kRotationDragSpeed = 0.5F;
constexpr float kScaleDragSpeed = 0.05F;
constexpr float kCameraRotateSpeed = 0.25F;
constexpr float kMinOrbitDistance = 0.01F;
constexpr float kMinNearPlane = 0.001F;
constexpr float kPitchLimitDegrees = 89.0F;
constexpr float kWindowScreenMargin = 24.0F;
constexpr float kInspectorWidth = 390.0F;
constexpr float kSectionSpacing = 10.0F;
constexpr float kLabelColumnWidth = 70.0F;
constexpr float kAxisButtonWidth = 24.0F;
constexpr float kListHeightRatio = 0.34F;
constexpr float kLightVectorDragSpeed = 0.05F;
constexpr float kLightIntensityDragSpeed = 0.05F;
constexpr float kConeAngleDragSpeed = 0.25F;
constexpr float kMouseOrbitSpeed = 0.006F;
constexpr float kMousePanSpeed = 0.0030F;
constexpr float kMouseZoomSpeed = 0.12F;
constexpr float kMouseDragThreshold = 1.5F;
static bool gShowObjectGizmo = true;
static ImGuizmo::OPERATION gObjectGizmoOperation = ImGuizmo::TRANSLATE;

///////////////////////////////////////////////
// Camera and Transform Utilities
///////////////////////////////////////////////

// Normalizes a vector and returns a fallback vector if the input vector is too small to normalize.
auto normalizeOrFallback(const glm::vec3 &vector, const glm::vec3 &fallback) -> glm::vec3
{
  const float vectorLength = glm::length(vector);
  if (vectorLength <= 0.0001F)
  {
    return fallback;
  }

  return vector / vectorLength;
}

// Computes the forward direction of the camera based on its position and target (center).
auto computeCameraForward(const Camera &camera) -> glm::vec3
{
  return normalizeOrFallback(camera.m_center - camera.params.cameraPos,
                             glm::vec3(0.0F, 1.0F, 0.0F));
}

// Computes the right direction of the camera based on its forward direction and an up hint.
auto computeCameraRight(const glm::vec3 &forward, const glm::vec3 &upHint) -> glm::vec3
{
  glm::vec3 right = glm::cross(forward, upHint);
  if (glm::length(right) <= 0.0001F)
  {
    right = glm::cross(forward, glm::vec3(0.0F, 0.0F, 1.0F));
  }

  return normalizeOrFallback(right, glm::vec3(1.0F, 0.0F, 0.0F));
}

// Computes the up direction of the camera based on its right and forward directions.
auto computeCameraUp(const glm::vec3 &right, const glm::vec3 &forward) -> glm::vec3
{
  return normalizeOrFallback(glm::cross(right, forward), glm::vec3(0.0F, 0.0F, 1.0F));
}

// Computes the yaw and pitch angles (in degrees) from a given forward direction vector.
auto computeYawPitchDegrees(const glm::vec3 &forward) -> glm::vec2
{
  const float clampedZ = std::clamp(forward.z, -1.0F, 1.0F);
  const float yawRadians = std::atan2(forward.y, forward.x);
  const float pitchRadians = std::asin(clampedZ);

  return {glm::degrees(yawRadians), glm::degrees(pitchRadians)};
}

// Computes the forward direction vector from given yaw and pitch angles (in degrees).
auto makeForwardFromYawPitchDegrees(float yawDegrees, float pitchDegrees) -> glm::vec3
{
  const float yawRadians = glm::radians(yawDegrees);
  const float pitchRadians = glm::radians(pitchDegrees);
  const float cosPitch = std::cos(pitchRadians);

  return normalizeOrFallback(glm::vec3(std::cos(yawRadians) * cosPitch,
                                       std::sin(yawRadians) * cosPitch, std::sin(pitchRadians)),
                             glm::vec3(0.0F, 1.0F, 0.0F));
}

// Rotates a vector around a given axis by a specified angle in radians.
auto rotateAroundAxis(const glm::vec3 &vector, const glm::vec3 &axis, float angleRadians)
    -> glm::vec3
{
  const glm::vec3 normalizedAxis = normalizeOrFallback(axis, glm::vec3(0.0F, 0.0F, 1.0F));
  const float cosAngle = std::cos(angleRadians);
  const float sinAngle = std::sin(angleRadians);
  return (vector * cosAngle) + (glm::cross(normalizedAxis, vector) * sinAngle) +
         (normalizedAxis * glm::dot(normalizedAxis, vector) * (1.0F - cosAngle));
}

///////////////////////////////////////////////
// Gizmo Utilities
///////////////////////////////////////////////

// Context for rendering gizmos in the viewport, including view and projection matrices, as well
// as the ImGui viewport and draw list.
struct GizmoViewportContext
{
  glm::mat4 viewMatrix;
  glm::mat4 projectionMatrix;
  const ImGuiViewport *viewport;
  ::ImDrawList *drawList;
};

// Creates a GizmoViewportContext for the given scene, which includes the view and projection
// matrices, as well as the ImGui viewport and draw list.
auto createGizmoViewportContext(Scene &scene) -> GizmoViewportContext
{
  // Retrieve the camera from the scene and compute the view and projection matrices.
  // Transpose the matrices as they are Vulkan-style column-major matrices, while ImGuizmo expects
  // row-major matrices. And flip the Y-axis of the projection matrix to match ImGui's coordinate
  // system.
  const auto &camera = scene.getCamera();
  const glm::mat4 viewMatrix = glm::transpose(camera.params.view);
  glm::mat4 projectionMatrix = glm::transpose(camera.params.proj);
  projectionMatrix[1][1] *= -1.0F;

  return GizmoViewportContext{.viewMatrix = viewMatrix,
                              .projectionMatrix = projectionMatrix,
                              .viewport = ::ImGui::GetMainViewport(),
                              .drawList = ::ImGui::GetForegroundDrawList()};
}

// Sets up the ImGuizmo viewport for rendering gizmos, including setting the draw list and the
// viewport rectangle.
auto setupGizmoViewport(const GizmoViewportContext &context) -> void
{
  ImGuizmo::SetOrthographic(false);
  ImGuizmo::SetDrawlist(context.drawList);
  ImGuizmo::SetRect(context.viewport->Pos.x, context.viewport->Pos.y, context.viewport->Size.x,
                    context.viewport->Size.y);
}

// Computes the screen-space position of a world-space point by projecting it using the provided
// view and projection matrices, and the viewport position and size.
auto projectWorldToScreen(const glm::vec3 &worldPosition, const glm::mat4 &viewMatrix,
                          const glm::mat4 &projectionMatrix, const ImVec2 &viewportPos,
                          const ImVec2 &viewportSize, ImVec2 &screenPosition) -> bool
{
  // Project the world position to clip space using the view and projection matrices.
  const glm::vec4 clipPosition = projectionMatrix * viewMatrix * glm::vec4(worldPosition, 1.0F);
  if (std::abs(clipPosition.w) <= 0.000001F)
  {
    return false;
  }

  // Convert the clip space position to normalized device coordinates (NDC).
  const glm::vec3 ndc = glm::vec3(clipPosition) / clipPosition.w;
  if (ndc.z < -1.0F || ndc.z > 1.0F)
  {
    return false;
  }

  // Convert the normalized device coordinates (NDC) to screen space coordinates.
  // The NDC coordinates are in the range [-1, 1], so we map them to the viewport size and position.
  // The Y-axis is flipped to match ImGui's coordinate system, where the origin is at the top-left.
  const float x = (((ndc.x + 1.0F) * 0.5F) * viewportSize.x) + viewportPos.x;
  const float y = (((1.0F - ndc.y) * 0.5F) * viewportSize.y) + viewportPos.y;
  screenPosition = ImVec2(x, y);

  return true;
}

// Draws a line in screen space between two world positions, projecting them using the provided view
// and projection matrices, and the viewport position and size.
auto drawProjectedLine(::ImDrawList *drawList, const glm::vec3 &start, const glm::vec3 &end,
                       const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix,
                       const ImVec2 &viewportPos, const ImVec2 &viewportSize, ImU32 color,
                       float thickness) -> void
{
  ImVec2 startScreen{};
  ImVec2 endScreen{};
  // Project the start and end world positions to screen space. If either projection fails, do not
  if (!projectWorldToScreen(start, viewMatrix, projectionMatrix, viewportPos, viewportSize,
                            startScreen) ||
      !projectWorldToScreen(end, viewMatrix, projectionMatrix, viewportPos, viewportSize,
                            endScreen))
  {
    return;
  }

  // Draw the line in screen space using the ImGui draw list.
  drawList->AddLine(startScreen, endScreen, color, thickness);
}

// Renders the controls for enabling/disabling light gizmos and selecting the operation mode
// (translate or rotate).
auto renderPerLightGizmoControls(
    uint8_t &enabled, std::optional<std::reference_wrapper<uint8_t>> operationMode = std::nullopt)
    -> void
{
  bool gizmoEnabled = enabled != 0;
  if (::ImGui::Checkbox("control in scene", &gizmoEnabled))
  {
    enabled = gizmoEnabled ? 1 : 0;
  }

  // Render radio buttons for selecting the operation mode (translate or rotate) if an operation
  // mode is provided.
  if (operationMode.has_value())
  {
    int mode = static_cast<int>(operationMode.value().get());
    if (::ImGui::RadioButton("translate", mode == 0))
    {
      mode = 0;
    }
    ::ImGui::SameLine();
    if (::ImGui::RadioButton("rotate", mode == 1))
    {
      mode = 1;
    }
    operationMode.value().get() = static_cast<uint8_t>(mode);
  }
}

// Renders a visual overlay for a spot light in the scene, including its position, direction, and
// cone angles.
auto drawSpotLightOverlay(::ImDrawList *drawList, const glm::mat4 &viewMatrix,
                          const glm::mat4 &projectionMatrix, const ImVec2 &viewportPos,
                          const ImVec2 &viewportSize, const glm::vec3 &lightPosition,
                          const glm::vec3 &lightDirection, float innerConeRadians,
                          float outerConeRadians, const glm::vec3 &cameraRight,
                          const glm::vec3 &cameraUp, const glm::vec3 &cameraForward) -> void
{
  constexpr float axisLength = 0.45F;
  constexpr float coneLength = 2.0F;
  constexpr float coneLineThickness = 1.7F;
  constexpr float axisLineThickness = 2.2F;
  constexpr int coneSegments = 40;

  // Camera-space triad centered at the selected light position.
  drawProjectedLine(drawList, lightPosition, lightPosition + cameraRight * axisLength, viewMatrix,
                    projectionMatrix, viewportPos, viewportSize, IM_COL32(235, 84, 84, 255),
                    axisLineThickness);
  drawProjectedLine(drawList, lightPosition, lightPosition + cameraUp * axisLength, viewMatrix,
                    projectionMatrix, viewportPos, viewportSize, IM_COL32(84, 230, 100, 255),
                    axisLineThickness);
  drawProjectedLine(drawList, lightPosition, lightPosition + cameraForward * axisLength, viewMatrix,
                    projectionMatrix, viewportPos, viewportSize, IM_COL32(95, 153, 255, 255),
                    axisLineThickness);

  // Compute the cone geometry based on the light's position, direction, and cone angles.
  glm::vec3 direction = lightDirection;
  if (glm::length(direction) <= 0.0001F)
  {
    direction = glm::vec3(0.0F, 0.0F, -1.0F);
  }
  else
  {
    direction = glm::normalize(direction);
  }

  // Compute the tangent and bitangent vectors for the cone's circular base.
  glm::vec3 tangent = glm::cross(direction, glm::vec3(0.0F, 0.0F, 1.0F));
  if (glm::length(tangent) <= 0.0001F)
  {
    tangent = glm::cross(direction, glm::vec3(1.0F, 0.0F, 0.0F));
  }
  tangent = glm::normalize(tangent);
  const glm::vec3 bitangent = glm::normalize(glm::cross(direction, tangent));

  // Compute the positions of the inner and outer rings of the cone based on the cone angles and
  // length.
  const glm::vec3 coneCenter = lightPosition + direction * coneLength;
  const float innerRadius = std::tan(innerConeRadians) * coneLength;
  const float outerRadius = std::tan(outerConeRadians) * coneLength;

  // Precompute the positions of the inner and outer rings of the cone for efficient rendering.
  std::array<glm::vec3, coneSegments> innerRing{};
  std::array<glm::vec3, coneSegments> outerRing{};

  // Compute the positions of the inner and outer rings of the cone by iterating over the segments
  for (int segment = 0; segment < coneSegments; ++segment)
  {
    const float angle =
        (glm::two_pi<float>() * static_cast<float>(segment)) / static_cast<float>(coneSegments);
    const glm::vec3 radial = (std::cos(angle) * tangent) + (std::sin(angle) * bitangent);
    innerRing[segment] = coneCenter + radial * innerRadius;
    outerRing[segment] = coneCenter + radial * outerRadius;
  }

  // Draw the inner and outer rings of the cone by connecting the computed positions with lines.
  for (int segment = 0; segment < coneSegments; ++segment)
  {
    const int nextSegment = (segment + 1) % coneSegments;
    drawProjectedLine(drawList, innerRing[segment], innerRing[nextSegment], viewMatrix,
                      projectionMatrix, viewportPos, viewportSize, IM_COL32(255, 208, 86, 255),
                      coneLineThickness);
    drawProjectedLine(drawList, outerRing[segment], outerRing[nextSegment], viewMatrix,
                      projectionMatrix, viewportPos, viewportSize, IM_COL32(255, 126, 59, 255),
                      coneLineThickness);
  }

  // Draw lines from the light position to the inner and outer rings at regular intervals.
  for (int segment = 0; segment < 4; ++segment)
  {
    const int idx = segment * (coneSegments / 4);
    drawProjectedLine(drawList, lightPosition, innerRing[idx], viewMatrix, projectionMatrix,
                      viewportPos, viewportSize, IM_COL32(255, 208, 86, 210), 1.3F);
    drawProjectedLine(drawList, lightPosition, outerRing[idx], viewMatrix, projectionMatrix,
                      viewportPos, viewportSize, IM_COL32(255, 126, 59, 210), 1.3F);
  }
}

////////////////////////////////////////////////
// ImGui Rendering Utilities
////////////////////////////////////////////////

// Render the section title with an optional caption and a separator line.
auto renderSectionTitle(const char *title, const char *caption = nullptr) -> void
{
  // Render the title in a bold font style.
  ::ImGui::TextUnformatted(title);
  // If a caption is provided, render it in a disabled style below the title.
  if (caption != nullptr)
  {
    ::ImGui::TextDisabled("%s", caption);
  }
  // Add spacing after the title and caption for better visual separation.
  ::ImGui::Separator();
}

// Render a row with a label and a draggable float value, allowing the user to adjust the value
auto renderFloatRow(const char *label, float &value, float speed, float minValue, float maxValue,
                    const char *format) -> bool
{
  bool changed = false;
  ::ImGui::PushID(label);
  ::ImGui::AlignTextToFramePadding();
  ::ImGui::TextUnformatted(label);
  ::ImGui::SameLine(kLabelColumnWidth);
  ::ImGui::SetNextItemWidth(-1.0F);
  changed = ::ImGui::DragFloat("##value", &value, speed, minValue, maxValue, format);
  ::ImGui::PopID();
  return changed;
}

// Render a row with a label and a draggable vec3 value, allowing the user to adjust the value
auto renderVec3Editor(const char *label, glm::vec3 &value, float resetValue, float speed,
                      const char *format) -> bool
{
  bool changed = false;

  ::ImGui::PushID(label);
  ::ImGui::AlignTextToFramePadding();
  ::ImGui::TextUnformatted(label);
  ::ImGui::SameLine(kLabelColumnWidth);

  // Calculate the available width for the draggable fields and buttons
  const float availableWidth = ::ImGui::GetContentRegionAvail().x;
  const float spacing = ::ImGui::GetStyle().ItemSpacing.x;
  const float fieldWidth = (availableWidth - (3.0F * kAxisButtonWidth) - (5.0F * spacing)) / 3.0F;

  // Lambda function to render the controls for each axis (X, Y, Z) with a button to reset the value
  const auto renderAxisControl = [&](const char *axisLabel, float &axisValue,
                                     const ImVec4 &color) -> void
  {
    ::ImGui::PushStyleColor(ImGuiCol_Button, color);
    ::ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(color.x + 0.08F, color.y + 0.08F, color.z + 0.08F, 1.0F));
    ::ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            ImVec4(color.x + 0.14F, color.y + 0.14F, color.z + 0.14F, 1.0F));

    if (::ImGui::Button(axisLabel, ImVec2(kAxisButtonWidth, 0.0F)))
    {
      axisValue = resetValue;
      changed = true;
    }

    ::ImGui::PopStyleColor(3);
    ::ImGui::SameLine(0.0F, spacing);
    ::ImGui::SetNextItemWidth(fieldWidth);
    changed |= ::ImGui::DragFloat((std::string("##") + axisLabel).c_str(), &axisValue, speed, 0.0F,
                                  0.0F, format);
  };

  // Render the controls for each axis (X, Y, Z) with appropriate colors and spacing
  renderAxisControl("X", value.x, ImVec4(0.75F, 0.28F, 0.28F, 1.0F));
  ::ImGui::SameLine(0.0F, spacing);
  renderAxisControl("Y", value.y, ImVec4(0.30F, 0.65F, 0.30F, 1.0F));
  ::ImGui::SameLine(0.0F, spacing);
  renderAxisControl("Z", value.z, ImVec4(0.28F, 0.45F, 0.80F, 1.0F));

  ::ImGui::PopID();
  return changed;
}

////////////////////////////////////////////////
// Scene Camera Navigation Utilities (Mouse Input)
////////////////////////////////////////////////

auto handleMouseSceneCameraNavigation(Scene &scene) -> void
{
  ::ImGuiIO &io = ::ImGui::GetIO();

  // Avoid camera motion while interacting with UI controls or active gizmos.
  if (io.WantCaptureMouse || ImGuizmo::IsOver() || ImGuizmo::IsUsing())
  {
    return;
  }

  auto &camera = scene.getCamera();
  glm::vec3 cameraPosition = camera.params.cameraPos;
  glm::vec3 cameraTarget = camera.m_center;
  glm::vec3 cameraUp = normalizeOrFallback(camera.m_up, glm::vec3(0.0F, 0.0F, 1.0F));

  glm::vec3 forward = computeCameraForward(camera);
  glm::vec3 right = computeCameraRight(forward, cameraUp);
  glm::vec3 up = computeCameraUp(right, forward);
  float orbitDistance = glm::max(glm::distance(cameraPosition, cameraTarget), kMinOrbitDistance);

  bool cameraChanged = false;

  const bool isRightDrag = ::ImGui::IsMouseDragging(ImGuiMouseButton_Right, kMouseDragThreshold);
  const bool isLeftDrag = ::ImGui::IsMouseDragging(ImGuiMouseButton_Left, kMouseDragThreshold);

  // Touchpad mapping:
  // - Left click + drag: orbit camera
  // - Right click + drag: pan camera
  if (isLeftDrag && !isRightDrag)
  {
    const ImVec2 mouseDelta = io.MouseDelta;
    if (mouseDelta.x != 0.0F || mouseDelta.y != 0.0F)
    {
      const glm::vec3 worldUp(0.0F, 0.0F, 1.0F);
      glm::vec3 offset = cameraPosition - cameraTarget;

      // Arcball-like orbit: horizontal drag rotates around global up, vertical around camera
      // right.
      offset = rotateAroundAxis(offset, worldUp, -mouseDelta.x * kMouseOrbitSpeed);
      cameraUp = rotateAroundAxis(cameraUp, worldUp, -mouseDelta.x * kMouseOrbitSpeed);

      const glm::vec3 rotatedForward = normalizeOrFallback(-offset, forward);
      const glm::vec3 orbitRight = computeCameraRight(rotatedForward, cameraUp);
      const glm::vec3 candidateOffset =
          rotateAroundAxis(offset, orbitRight, -mouseDelta.y * kMouseOrbitSpeed);
      const glm::vec3 candidateForward = normalizeOrFallback(-candidateOffset, rotatedForward);

      // Keep motion natural near poles without explicit pitch angles.
      if (std::abs(glm::dot(candidateForward, worldUp)) < 0.995F)
      {
        offset = candidateOffset;
        cameraUp = rotateAroundAxis(cameraUp, orbitRight, -mouseDelta.y * kMouseOrbitSpeed);
      }

      cameraPosition = cameraTarget + offset;
      forward = normalizeOrFallback(cameraTarget - cameraPosition, forward);
      right = computeCameraRight(forward, cameraUp);
      up = computeCameraUp(right, forward);
      cameraUp = up;
      cameraChanged = true;
    }
  }

  if (isRightDrag)
  {
    const ImVec2 mouseDelta = io.MouseDelta;
    if (mouseDelta.x != 0.0F || mouseDelta.y != 0.0F)
    {
      const float panScale = orbitDistance * kMousePanSpeed;
      const glm::vec3 panDelta = ((-right * mouseDelta.x) + (up * mouseDelta.y)) * panScale;
      cameraPosition += panDelta;
      cameraTarget += panDelta;
      cameraChanged = true;
    }
  }

  if (io.MouseWheel != 0.0F)
  {
    // Use the latest camera vectors (after orbit/pan in the same frame) for coherent zoom.
    forward = normalizeOrFallback(cameraTarget - cameraPosition, forward);
    const float zoomFactor = glm::max(0.1F, 1.0F - (io.MouseWheel * kMouseZoomSpeed));
    orbitDistance = glm::max(kMinOrbitDistance, orbitDistance * zoomFactor);
    cameraPosition = cameraTarget - forward * orbitDistance;
    cameraChanged = true;
  }

  if (cameraChanged)
  {
    camera.transform({.position = cameraPosition,
                      .center = cameraTarget,
                      .up = normalizeOrFallback(cameraUp, glm::vec3(0.0F, 0.0F, 1.0F)),
                      .fov = camera.m_fov,
                      .nearPlane = glm::max(camera.m_nearPlane, kMinNearPlane),
                      .farPlane = glm::max(camera.m_farPlane, camera.m_nearPlane + kMinNearPlane)});
  }
}

/////////////////////////////////////////////////
// Scene Object Inspector Window
/////////////////////////////////////////////////

// Builds a transformation matrix from the given Transform object, which includes position,
// rotation, and scale. The matrix is constructed in the order of translation, rotation (X, Y, Z),
// and scale.
auto buildTransformMatrix(const Transform &transform) -> glm::mat4
{
  glm::mat4 rotation = glm::mat4_cast(transform.rotation);

  glm::mat4 model = glm::translate(glm::mat4(1.0F), transform.position) * rotation *
                    glm::scale(glm::mat4(1.0F), transform.scale);

  return model;
}

// Applies the object gizmo to the selected object in the scene, allowing users to manipulate the
// object's transform (position, rotation, scale) directly in the viewport using ImGuizmo.
auto applyObjectGizmo(Scene &scene, std::optional<uint32_t> &selectedObjectId) -> void
{
  // If the object gizmo is disabled or no object is selected, we skip applying the gizmo.
  if (!gShowObjectGizmo || !selectedObjectId.has_value())
  {
    return;
  }

  // Find the selected object in the scene using its ID. If the object is not found, we reset the
  // selectedObjectId and return early.
  SceneObject *selectedObject =
      std::ranges::find_if(
          scene.getObjects(), [&](const std::unique_ptr<SceneObject> &objectPtr) -> bool
          { return objectPtr != nullptr && objectPtr->getObjectId() == *selectedObjectId; })
          ->get();
  if (selectedObject == nullptr)
  {
    selectedObjectId.reset();
    return;
  }

  // Create a GizmoViewportContext for the scene, which includes the view and projection matrices,
  // as well as the ImGui viewport and draw list.
  const GizmoViewportContext gizmoContext = createGizmoViewportContext(scene);

  // Retrieve the current transform of the selected object and build its transformation matrix for
  // use with ImGuizmo.
  Transform selectedTransform = selectedObject->getTransform();
  glm::mat4 objectMatrix = buildTransformMatrix(selectedTransform);
  setupGizmoViewport(gizmoContext);

  // Use ImGuizmo to manipulate the selected object's transform in the viewport. If the gizmo is
  // used, we decompose the resulting transformation matrix back into position, rotation, and scale
  // components and update the selected object's transform accordingly.
  if (ImGuizmo::Manipulate(glm::value_ptr(gizmoContext.viewMatrix),
                           glm::value_ptr(gizmoContext.projectionMatrix), gObjectGizmoOperation,
                           ImGuizmo::WORLD, glm::value_ptr(objectMatrix)))
  {
    std::array<float, 3> translation = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> rotation = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> scale = {1.0F, 1.0F, 1.0F};
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(objectMatrix), translation.data(),
                                          rotation.data(), scale.data());

    glm::mat4 rotationMatrix = objectMatrix;

    // Remove scale
    rotationMatrix[0] = glm::normalize(rotationMatrix[0]);
    rotationMatrix[1] = glm::normalize(rotationMatrix[1]);
    rotationMatrix[2] = glm::normalize(rotationMatrix[2]);

    glm::quat quaternion = glm::quat_cast(rotationMatrix);

    // Update the selected object's transform with the new position, rotation, and scale values,
    // ensuring that the scale is clamped to a minimum value to avoid degenerate cases.
    selectedTransform.position = glm::vec3(translation[0], translation[1], translation[2]);
    selectedTransform.rotation = quaternion;
    selectedTransform.scale =
        glm::max(glm::vec3(0.001F, 0.001F, 0.001F), glm::vec3(scale[0], scale[1], scale[2]));

    // Apply the updated transform to the selected object in the scene.
    selectedObject->transform(selectedTransform);
  }
}

// Renders the inspector window for the selected object in the scene, allowing users to view and
// edit the object's transform properties (position, rotation, scale) directly in the ImGui
// interface.
auto renderSelectedObjectInspector(SceneObject *selectedObject) -> void
{
  if (selectedObject == nullptr)
  {
    return;
  }

  // Render the section title for the selected object, indicating that users can edit the transform
  // directly in the inspector.
  renderSectionTitle(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " Selected Object",
                     "Edit transform directly in the inspector.");

  // Retrieve the current transform of the selected object and initialize a flag to track if any
  // changes are made to the transform properties.
  Transform updatedTransform = selectedObject->getTransform();
  // Convert Quaternion rotation to Euler angles in degrees for display and editing in the
  // inspector.
  glm::mat4 R = glm::mat4_cast(updatedTransform.rotation);
  float x, y, z;
  glm::extractEulerAngleXYZ(R, x, y, z);
  glm::vec3 rotationEuler = glm::degrees(glm::vec3(x, y, z));

  // Render the controls for editing the position, rotation, and scale of the selected object.
  bool transformChanged = false;
  transformChanged |=
      renderVec3Editor("Position", updatedTransform.position, 0.0F, kPositionDragSpeed, "%.2f");
  transformChanged |= renderVec3Editor("Rotation", rotationEuler, 0.0F, kRotationDragSpeed, "%.1f");
  transformChanged |=
      renderVec3Editor("Scale", updatedTransform.scale, 1.0F, kScaleDragSpeed, "%.2f");
  ::ImGui::Separator();

  // Update the rotation in the updatedTransform with the modified Euler angles, converting them
  // back to a Quaternion representation.
  updatedTransform.rotation = glm::quat(glm::eulerAngleXYZ(
      glm::radians(rotationEuler.x), glm::radians(rotationEuler.y), glm::radians(rotationEuler.z)));

  // Render the controls for enabling/disabling the object gizmo and selecting the operation mode
  ::ImGui::Checkbox("control in scene", &gShowObjectGizmo);
  if (::ImGui::RadioButton("translate", gObjectGizmoOperation == ImGuizmo::TRANSLATE))
  {
    gObjectGizmoOperation = ImGuizmo::TRANSLATE;
  }
  ::ImGui::SameLine();
  if (::ImGui::RadioButton("rotate", gObjectGizmoOperation == ImGuizmo::ROTATE))
  {
    gObjectGizmoOperation = ImGuizmo::ROTATE;
  }
  ::ImGui::SameLine();
  if (::ImGui::RadioButton("scale", gObjectGizmoOperation == ImGuizmo::SCALE))
  {
    gObjectGizmoOperation = ImGuizmo::SCALE;
  }

  // If any of the transform properties were changed, we update the selected object's transform.
  if (transformChanged)
  {
    selectedObject->transform(updatedTransform);
  }
}

// Renders the inspector window for the selected object in the scene, allowing users to view and
// edit the object's transform properties (position, rotation, scale) directly in the ImGui
// interface.
auto renderSelectedObjectTransformWindow(Scene &scene, std::optional<uint32_t> &selectedObjectId)
    -> void
{
  // If no object is selected, we skip rendering the inspector window.
  if (!selectedObjectId.has_value())
  {
    return;
  }

  // Find the selected object in the scene using its ID. If the object is not found, we reset the
  // selectedObjectId and return early.
  SceneObject *selectedObject =
      std::ranges::find_if(
          scene.getObjects(), [&](const std::unique_ptr<SceneObject> &objectPtr) -> bool
          { return objectPtr != nullptr && objectPtr->getObjectId() == *selectedObjectId; })
          ->get();

  if (selectedObject == nullptr)
  {
    selectedObjectId.reset();
    return;
  }

  // Render the inspector window for the selected object.
  bool isWindowOpen = true;
  ::ImGui::SetNextWindowSize(ImVec2(370.0F, 290.0F), ImGuiCond_Appearing);
  ::ImGui::Begin(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " Object transform", &isWindowOpen);
  renderSelectedObjectInspector(selectedObject);
  ::ImGui::End();

  // If the inspector window is closed by the user, we reset the selectedObjectId to indicate that
  // no object is currently selected.
  if (!isWindowOpen)
  {
    selectedObjectId.reset();
  }

  // Apply the object gizmo to the selected object, allowing users to manipulate its transform
  // directly in the scene viewport.
  applyObjectGizmo(scene, selectedObjectId);
}

/////////////////////////////////////////////////
// Light Rendering
/////////////////////////////////////////////////

// Renders the directional lights section in the ImGui interface, allowing users to view and edit
// the properties of directional lights in the scene.
auto renderDirectionalLightsSection(Scene &scene) -> void
{
  // Retrieve the list of directional lights from the scene and create a header label that
  // displays the number of directional lights.
  const auto &directionalLights = scene.getDirectionalLights();
  const std::string headerLabel = std::string(ICON_FA_SUN) + " Directional Lights (" +
                                  std::to_string(directionalLights.size()) + ")";

  // If the user collapses the header, we skip rendering the rest of the section.
  if (!::ImGui::CollapsingHeader(headerLabel.c_str()))
  {
    return;
  }

  // Push a unique ID for the directional lights section to avoid ID conflicts with other
  // sections.
  ::ImGui::PushID("directional-lights-section");

  // Iterate through each directional light in the scene and render its properties in a tree node.
  for (size_t lightIndex = 0; lightIndex < directionalLights.size(); ++lightIndex)
  {
    // Get a reference to the current directional light and create a label for it.
    auto &light = *directionalLights[lightIndex];
    const std::string lightLabel = "Directional Light " + std::to_string(lightIndex);

    // If the user collapses the tree node for this light, we skip rendering its properties.
    if (!::ImGui::TreeNode(lightLabel.c_str()))
    {
      continue;
    }

    // Get the current direction, color, and intensity of the light from its parameters.
    glm::vec3 direction(light.params.direction.x, light.params.direction.y,
                        light.params.direction.z);
    glm::vec3 color(light.params.color.x, light.params.color.y, light.params.color.z);
    float intensity = light.params.color.w;

    // Render the controls for editing the light's direction, color, and intensity.
    bool changed = false;
    changed |= ::ImGui::DragFloat3("direction", &direction.x, kLightVectorDragSpeed);
    changed |= ::ImGui::ColorEdit3("color", &color.x, ImGuiColorEditFlags_Float);
    changed |=
        ::ImGui::DragFloat("intensity", &intensity, kLightIntensityDragSpeed, 0.0F, 100.0F, "%.2f");

    // If any of the light's properties were changed, update the light's parameters accordingly.
    if (changed)
    {
      light.transform({.direction = direction, .color = color, .intensity = intensity});
    }

    // Pop the tree node for this light to close it in the ImGui interface.
    ::ImGui::TreePop();
  }

  // Pop the unique ID for the directional lights section to restore the previous ID context.
  ::ImGui::PopID();
}

// Renders the point lights section in the ImGui interface, allowing users to view and edit
// the properties of point lights in the scene, as well as manipulate their positions using
// gizmos.
auto renderPointLightsSection(Scene &scene) -> void
{
  // Retrieve the list of point lights from the scene and create a header label that
  // displays the number of point lights.
  const auto &pointLights = scene.getPointLights();
  const std::string headerLabel =
      std::string(ICON_FA_LIGHTBULB) + " Point Lights (" + std::to_string(pointLights.size()) + ")";

  // If the user collapses the header, we skip rendering the rest of the section.
  const bool pointSectionOpen =
      ::ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

  // Static vectors to store the enabled state and operation mode for each point light gizmo.
  static std::vector<uint8_t> pointGizmoEnabled;

  // Resize the vectors to match the number of point lights in the scene, initializing them to 0.
  pointGizmoEnabled.resize(pointLights.size(), 0);

  // If the point lights section is open, render the controls for each point light.
  if (pointSectionOpen)
  {
    // Push a unique ID for the point lights section to avoid ID conflicts with other sections.
    ::ImGui::PushID("point-lights-section");

    // Iterate through each point light in the scene and render its properties in a tree node.
    for (size_t lightIndex = 0; lightIndex < pointLights.size(); ++lightIndex)
    {
      auto &light = *pointLights[lightIndex];
      const std::string lightLabel = "Point Light " + std::to_string(lightIndex);

      // If the user collapses the tree node for this light, we skip rendering its properties.
      if (!::ImGui::TreeNode(lightLabel.c_str()))
      {
        continue;
      }

      // Render the controls for enabling/disabling the gizmo and selecting the operation mode
      // (translate or rotate).
      renderPerLightGizmoControls(pointGizmoEnabled[lightIndex]);

      // Get the current position, color, and intensity of the light from its parameters.
      glm::vec3 position(light.params.position.x, light.params.position.y, light.params.position.z);
      glm::vec3 color(light.params.color.x, light.params.color.y, light.params.color.z);
      float intensity = light.params.color.w;

      // Render the controls for editing the light's position, color, and intensity.
      bool changed = false;
      changed |= ::ImGui::DragFloat3("position", &position.x, kLightVectorDragSpeed);
      changed |= ::ImGui::ColorEdit3("color", &color.x, ImGuiColorEditFlags_Float);
      changed |= ::ImGui::DragFloat("intensity", &intensity, kLightIntensityDragSpeed, 0.0F, 100.0F,
                                    "%.2f");

      // If any of the light's properties were changed, update the light's parameters accordingly.
      if (changed)
      {
        light.transform({.position = position, .color = color, .intensity = intensity});
      }

      // Pop the tree node for this light to close it in the ImGui interface.
      ::ImGui::TreePop();
    }

    // Pop the unique ID for the point lights section to restore the previous ID context.
    ::ImGui::PopID();
  }

  // Check if any point light gizmo is enabled for manipulation in the scene.
  const bool hasAnyPointGizmo =
      std::ranges::any_of(pointGizmoEnabled, [](uint8_t value) { return value != 0; });

  // If any point light gizmo is enabled, set up the gizmo viewport and handle manipulation.
  if (hasAnyPointGizmo)
  {
    // Create a GizmoViewportContext for the scene, which includes the view and projection
    // matrices, as well as the ImGui viewport and draw list.
    const GizmoViewportContext gizmoContext = createGizmoViewportContext(scene);
    setupGizmoViewport(gizmoContext);

    // Iterate through each point light and apply the gizmo manipulation if it is enabled.
    for (size_t lightIndex = 0; lightIndex < pointLights.size(); ++lightIndex)
    {
      // Skip the light if its gizmo is not enabled.
      if (pointGizmoEnabled[lightIndex] == 0)
      {
        continue;
      }

      // Get a reference to the current point light and its position.
      auto &light = *pointLights[lightIndex];
      glm::vec3 lightPosition(light.params.position.x, light.params.position.y,
                              light.params.position.z);

      // Create a transformation matrix for the light's position, which will be manipulated by the
      // gizmo.
      glm::mat4 lightMatrix(1.0F);
      lightMatrix[3] = glm::vec4(lightPosition, 1.0F);

      // Push a unique ID for the gizmo manipulation to avoid ID conflicts with other gizmos. 1000
      // is added to the light index to ensure uniqueness.
      ImGuizmo::PushID(static_cast<int>(1000 + lightIndex));

      // Use ImGuizmo to manipulate the light's transformation matrix based on the view and
      // projection matrices, and the world space.
      // If the manipulation is successful, update the light's position accordingly.
      if (ImGuizmo::Manipulate(glm::value_ptr(gizmoContext.viewMatrix),
                               glm::value_ptr(gizmoContext.projectionMatrix), ImGuizmo::TRANSLATE,
                               ImGuizmo::WORLD, glm::value_ptr(lightMatrix)))
      {
        const glm::vec3 updatedPosition = glm::vec3(lightMatrix[3]);
        light.transform({.position = updatedPosition});
      }
      // Pop the unique ID for the gizmo manipulation to restore the previous ID context.
      ImGuizmo::PopID();
    }
  }
}

// Renders the spot lights section in the ImGui interface, allowing users to view and edit
// the properties of spot lights in the scene, as well as manipulate their positions and directions
// using gizmos.
auto renderSpotLightsSection(Scene &scene) -> void
{
  // Retrieve the list of spot lights from the scene and create a header label that
  // displays the number of spot lights.
  const auto &spotLights = scene.getSpotLights();
  const std::string headerLabel =
      std::string(ICON_FA_VIDEO) + " Spot Lights (" + std::to_string(spotLights.size()) + ")";

  // Check if the user has expanded the spot lights section in the ImGui interface. If not, we skip
  // rendering the rest of the section.
  const bool spotSectionOpen =
      ::ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

  // Static vectors to store the enabled state and operation mode for each spot light gizmo.
  static std::vector<uint8_t> spotGizmoEnabled;
  static std::vector<uint8_t> spotGizmoOperationMode;

  // Resize the vectors to match the number of spot lights in the scene, initializing them to 0.
  spotGizmoEnabled.resize(spotLights.size(), 0);
  spotGizmoOperationMode.resize(spotLights.size(), 0);

  if (spotSectionOpen)
  {
    // Push a unique ID for the spot lights section to avoid ID conflicts with other sections.
    ::ImGui::PushID("spot-lights-section");

    // Iterate through each spot light in the scene and render its properties in a tree node.
    for (size_t lightIndex = 0; lightIndex < spotLights.size(); ++lightIndex)
    {
      // Get a reference to the current spot light and create a label for it.
      auto &light = *spotLights[lightIndex];
      const std::string lightLabel = "Light " + std::to_string(lightIndex);

      // If the user collapses the tree node for this light, we skip rendering its properties.
      if (!::ImGui::TreeNode(lightLabel.c_str()))
      {
        continue;
      }

      // Render the controls for enabling/disabling the gizmo and selecting the operation mode
      // (translate or rotate).
      renderPerLightGizmoControls(spotGizmoEnabled[lightIndex],
                                  std::ref(spotGizmoOperationMode[lightIndex]));

      // Get the current position, direction, color, intensity, and cone angles of the light from
      // its parameters.
      glm::vec3 position(light.params.position.x, light.params.position.y, light.params.position.z);
      glm::vec3 direction(light.params.direction.x, light.params.direction.y,
                          light.params.direction.z);
      glm::vec3 color(light.params.color.x, light.params.color.y, light.params.color.z);
      float intensity = light.params.color.w;

      // Convert the inner and outer cone angles from cosine values to degrees for display in the
      // ImGui interface.
      const float innerConeCos = std::clamp(light.params.position.w, -1.0F, 1.0F);
      const float outerConeCos = std::clamp(light.params.direction.w, -1.0F, 1.0F);
      float innerConeDegrees = glm::degrees(std::acos(innerConeCos));
      float outerConeDegrees = glm::degrees(std::acos(outerConeCos));

      // Render the controls for editing the light's position, direction, color, intensity, and cone
      // angles.
      bool changed = false;
      changed |= ::ImGui::DragFloat3("position", &position.x, kLightVectorDragSpeed);
      changed |= ::ImGui::DragFloat3("direction", &direction.x, kLightVectorDragSpeed);
      changed |= ::ImGui::ColorEdit3("color", &color.x, ImGuiColorEditFlags_Float);
      changed |= ::ImGui::DragFloat("intensity", &intensity, kLightIntensityDragSpeed, 0.0F, 100.0F,
                                    "%.2f");
      changed |= ::ImGui::SliderFloat("inner cone", &innerConeDegrees, 0.0F, 89.0F, "%.2f deg");
      changed |= ::ImGui::SliderFloat("outer cone", &outerConeDegrees, 0.0F, 89.9F, "%.2f deg");

      // Clamp the inner and outer cone angles to ensure they are within valid ranges and maintain
      // the relationship that the inner cone angle is less than or equal to the outer cone angle.
      const float clampedOuterConeDegrees = glm::max(outerConeDegrees, 0.0F);
      const float clampedInnerConeDegrees =
          std::clamp(innerConeDegrees, 0.0F, clampedOuterConeDegrees);
      const bool coneWasClamped = (clampedInnerConeDegrees != innerConeDegrees) ||
                                  (clampedOuterConeDegrees != outerConeDegrees);

      innerConeDegrees = clampedInnerConeDegrees;
      outerConeDegrees = clampedOuterConeDegrees;

      // If any of the light's properties have changed or the cone angles were clamped, update the
      // light's transform.
      if (changed || coneWasClamped)
      {
        light.transform({.position = position,
                         .direction = direction,
                         .color = color,
                         .intensity = intensity,
                         .innerCone = glm::radians(innerConeDegrees),
                         .outerCone = glm::radians(outerConeDegrees)});
      }
      // Pop the tree node for this light to close it in the ImGui interface.
      ::ImGui::TreePop();
    }
    // Pop the unique ID for the spot lights section to restore the previous ID context.
    ::ImGui::PopID();
  }

  // Check if any spot light gizmo is enabled for manipulation in the scene.
  const bool hasAnySpotGizmo =
      std::ranges::any_of(spotGizmoEnabled, [](uint8_t value) { return value != 0; });

  if (hasAnySpotGizmo)
  {
    // Create a GizmoViewportContext for the scene, which includes the view and projection
    // matrices, as well as the ImGui viewport and draw list.
    const GizmoViewportContext gizmoContext = createGizmoViewportContext(scene);

    // Spot overlays are line-rendered in screen space, so they use the same context as gizmos.
    // Setup the ImGuizmo viewport for rendering gizmos, including setting the draw list and the
    // viewport rectangle.
    const glm::mat4 &viewMatrix = gizmoContext.viewMatrix;
    const glm::mat4 &projectionMatrix = gizmoContext.projectionMatrix;
    const auto &camera = scene.getCamera();
    const glm::vec3 cameraForward = computeCameraForward(camera);
    const glm::vec3 cameraRight = computeCameraRight(cameraForward, camera.m_up);
    const glm::vec3 cameraUp = computeCameraUp(cameraRight, cameraForward);
    setupGizmoViewport(gizmoContext);

    // Iterate through each spot light and apply the gizmo manipulation if it is enabled.
    for (size_t lightIndex = 0; lightIndex < spotLights.size(); ++lightIndex)
    {
      // Skip the light if its gizmo is not enabled.
      if (spotGizmoEnabled[lightIndex] == 0)
      {
        continue;
      }

      // Get a reference to the current spot light and its position and direction.
      auto &light = *spotLights[lightIndex];
      glm::vec3 lightPosition(light.params.position.x, light.params.position.y,
                              light.params.position.z);
      glm::vec3 lightDirection(light.params.direction.x, light.params.direction.y,
                               light.params.direction.z);
      lightDirection = normalizeOrFallback(lightDirection, glm::vec3(0.0F, 0.0F, -1.0F));

      // Compute the right and up vectors for the light's local coordinate system, ensuring they are
      // orthogonal to the light's direction. If the light's direction is nearly parallel to the
      // world up vector, we use an alternative axis to compute the right vector.
      glm::vec3 worldUp(0.0F, 0.0F, 1.0F);
      glm::vec3 right = glm::cross(worldUp, lightDirection);
      if (glm::length(right) <= 0.0001F)
      {
        right = glm::cross(glm::vec3(1.0F, 0.0F, 0.0F), lightDirection);
      }
      right = glm::normalize(right);
      glm::vec3 up = glm::normalize(glm::cross(lightDirection, right));

      // Construct a transformation matrix for the light's position and orientation, which will be
      // manipulated by the gizmo. The matrix is built using the right, up, and light direction
      // vectors, along with the light's position.
      glm::mat4 lightMatrix(1.0F);
      lightMatrix[0] = glm::vec4(right, 0.0F);
      lightMatrix[1] = glm::vec4(up, 0.0F);
      lightMatrix[2] = glm::vec4(lightDirection, 0.0F);
      lightMatrix[3] = glm::vec4(lightPosition, 1.0F);

      // Calculate the inner and outer cone angles in radians from the light's parameters, ensuring
      // they are clamped to the valid range of [-1, 1] before applying the arccosine function.
      const float innerConeRadians = std::acos(std::clamp(light.params.position.w, -1.0F, 1.0F));
      const float outerConeRadians = std::acos(std::clamp(light.params.direction.w, -1.0F, 1.0F));

      // Draw the spot light overlay in the scene using the computed parameters, including the view
      // and projection matrices, the viewport position and size, the light's position and
      // direction, the inner and outer cone angles, and the camera's right, up, and forward
      // vectors. This overlay visually represents the spot light's cone in the scene.
      drawSpotLightOverlay(gizmoContext.drawList, viewMatrix, projectionMatrix,
                           gizmoContext.viewport->Pos, gizmoContext.viewport->Size, lightPosition,
                           lightDirection, innerConeRadians, outerConeRadians, cameraRight,
                           cameraUp, cameraForward);

      // Determine the operation mode for the gizmo based on the user's selection. If the user has
      // selected the rotate mode, we use ImGuizmo::ROTATE; otherwise, we use ImGuizmo::TRANSLATE
      // for position manipulation.
      const ImGuizmo::OPERATION operation =
          (spotGizmoOperationMode[lightIndex] == 1) ? ImGuizmo::ROTATE : ImGuizmo::TRANSLATE;

      // Push a unique ID for the gizmo manipulation to avoid ID conflicts with other gizmos. 2000
      // is added to the light index to ensure uniqueness.
      ImGuizmo::PushID(static_cast<int>(2000 + lightIndex));

      // Use ImGuizmo to manipulate the light's transformation matrix based on the view and
      // projection matrices, the selected operation mode, and the world space. If the manipulation
      // is successful, update the light's position and direction accordingly.
      if (ImGuizmo::Manipulate(glm::value_ptr(gizmoContext.viewMatrix),
                               glm::value_ptr(gizmoContext.projectionMatrix), operation,
                               ImGuizmo::WORLD, glm::value_ptr(lightMatrix)))
      {
        auto updatedPosition = glm::vec3(lightMatrix[3]);
        glm::vec3 updatedDirection = normalizeOrFallback(glm::vec3(lightMatrix[2]), lightDirection);
        light.transform({.position = updatedPosition, .direction = updatedDirection});
      }
      // Pop the unique ID for the gizmo manipulation to restore the previous ID context.
      ImGuizmo::PopID();
    }
  }
}

///////////////////////////////////////////////////
// Inspector Rendering
///////////////////////////////////////////////////

// Render the list of objects in the scene, allowing selection and visibility toggling.
auto renderObjectList(Scene &scene, std::optional<uint32_t> &selectedObjectId) -> void
{
  // Get the list of scene objects and initialize a flag to track if the selected object exists.
  const auto &sceneObjects = scene.getObjects();
  bool selectedObjectExists = false;

  // Render the section title for the scene object list, with an icon and caption.
  renderSectionTitle(ICON_FA_CUBES " Scene", "Toggle visibility and inspect objects.");

  // Calculate the height of the child window for the object list, ensuring a minimum height.
  const float childHeight = glm::max(140.0F, ::ImGui::GetWindowHeight() * kListHeightRatio);
  // Begin a child window for the scene object list, with borders and the calculated height.
  ::ImGui::BeginChild("scene-object-list", ImVec2(0.0F, childHeight), ImGuiChildFlags_Borders);

  // If there are no scene objects, display a message and reset the selected object ID.
  if (sceneObjects.empty())
  {
    ::ImGui::TextDisabled(ICON_FA_CIRCLE_INFO " No scene objects available.");
    selectedObjectId.reset();
    ::ImGui::EndChild();
    return;
  }

  // Iterate through each scene object, rendering its information and controls.
  for (const auto &objectPtr : sceneObjects)
  {
    if (!objectPtr)
    {
      continue;
    }

    // Get the reference to the scene object and its unique ID.
    auto &sceneObject = *objectPtr;
    const uint32_t objectId = sceneObject.getObjectId();

    // Determine if the current object is selected by comparing its ID with the selected object
    // ID.
    const bool isSelected = selectedObjectId.has_value() && *selectedObjectId == objectId;

    // If the object is selected, set the flag indicating that the selected object exists.
    if (isSelected)
    {
      // This checks every frame if the selected object is still present in the scene. If it is,
      // we keep the selection; if not, we reset the selection at the end to avoid dangling
      // references.
      selectedObjectExists = true;
    }

    // Push a unique ID for the current object to avoid ID collisions in ImGui. This ensures that
    // each object has a unique identifier in the UI.
    ::ImGui::PushID(static_cast<int>(objectId));

    // Fade hidden objects
    if (!sceneObject.isVisible())
    {
      // If the object is not visible, change the text color to a faded gray to indicate its
      // hidden state.
      ::ImGui::PushStyleColor(::ImGuiCol_Text, ::ImVec4(0.45f, 0.45f, 0.45f, 1.0f));
    }

    // Create a label for the object, including its mesh ID if available, or a default label with
    // the object ID.
    std::string label;
    const auto &meshId = sceneObject.getMeshId();
    if (!meshId.empty())
    {
      label = std::string(ICON_FA_CUBE) + " " + meshId;
    }
    else
    {
      label = std::string(ICON_FA_CIRCLE) + " Object " + std::to_string(objectId);
    }

    // Determine the visibility icon based on the object's visibility state. If the object is
    // visible, use an eye icon; otherwise, use an eye-slash icon.
    const char *visibilityIcon = sceneObject.isVisible() ? ICON_FA_EYE : ICON_FA_EYE_SLASH;

    // Calculate the width of the visibility button and the available width for the selectable
    // item. This ensures that the visibility button and the object label fit within the available
    // space.
    const ::ImGuiStyle &style = ::ImGui::GetStyle();
    const float visibilityButtonWidth =
        ::ImGui::CalcTextSize(visibilityIcon).x + (style.FramePadding.x * 2.0F);
    const float availableRowWidth = ::ImGui::GetContentRegionAvail().x;
    const float selectableWidth =
        availableRowWidth > (visibilityButtonWidth + style.ItemSpacing.x)
            ? availableRowWidth - visibilityButtonWidth - style.ItemSpacing.x
            : 0.0F;

    // Render a selectable item for the object label, allowing the user to select or deselect the
    // object. If the item is selected, update the selected object ID accordingly.
    if (::ImGui::Selectable(label.c_str(), isSelected, 0, ::ImVec2(selectableWidth, 0.0F)))
    {
      // Toggle selection state: if the object is already selected, deselect it; otherwise, select
      // it.
      if (isSelected)
      {
        selectedObjectId.reset();
        selectedObjectExists = false;
      }
      else
      {
        selectedObjectId = objectId;
        selectedObjectExists = true;
      }
    }

    // Show tooltip with object details when hovered over the selectable item. This provides
    // additional information about the object, such as its ID, mesh ID, and visibility state.
    if (::ImGui::IsItemHovered())
    {
      ::ImGui::BeginTooltip();

      ::ImGui::Text(ICON_FA_TAG " ID: %u", objectId);
      ::ImGui::Text(ICON_FA_CUBE " Mesh: %s", meshId.empty() ? "None" : meshId.c_str());
      ::ImGui::Text(ICON_FA_EYE " Visible: %s", sceneObject.isVisible() ? "Yes" : "No");

      ::ImGui::EndTooltip();
    }

    // If the object is not visible, pop the style color to restore the original text color. This
    // ensures that the text color is only changed for hidden objects and is reset for visible
    // objects.
    if (!sceneObject.isVisible())
    {
      ::ImGui::PopStyleColor();
    }

    ::ImGui::SameLine();

    // Render a small button for toggling the visibility of the object. When clicked, it updates
    // the visibility state of the object and changes the icon accordingly.
    if (::ImGui::SmallButton(visibilityIcon))
    {
      sceneObject.setVisible(!sceneObject.isVisible());
    }

    ::ImGui::PopID();
  }

  // Selection cleanup
  if (!selectedObjectExists)
  {
    selectedObjectId.reset();
  }

  ::ImGui::EndChild();
}

// Render the camera section in the inspector, allowing users to adjust camera parameters.
auto renderCameraSection(Scene &scene) -> void
{
  auto &camera = scene.getCamera();

  // Extract camera parameters for easier manipulation and calculations. These are cpu-side
  // representations of the camera's position, target, and up vector. The cameras gpu-side uniform
  // buffer gets updated every frame, by copying the cpu-side camera parameters to the gpu-side
  // host-visible uniform buffer.
  glm::vec3 cameraPosition = camera.params.cameraPos;
  glm::vec3 cameraTarget = camera.m_center;
  // The camera's up vector is normalized, and if it is invalid (zero length), a fallback up
  // vector is used (0, 0, 1). This ensures that the camera has a valid up direction for
  // orientation calculations.
  glm::vec3 cameraUp = normalizeOrFallback(camera.m_up, glm::vec3(0.0F, 0.0F, 1.0F));

  // Compute the camera's forward, right, and up axes based on its position, target, and up
  // vector.
  glm::vec3 forwardAxis = computeCameraForward(camera);
  glm::vec3 rightAxis = computeCameraRight(forwardAxis, cameraUp);
  glm::vec3 upAxis = computeCameraUp(rightAxis, forwardAxis);

  // Compute the camera's yaw and pitch angles in degrees based on its forward axis. These angles
  // are used for orbiting and rotating the camera around its target point.
  glm::vec2 yawPitch = computeYawPitchDegrees(forwardAxis);
  float orbitDistance = glm::max(glm::distance(cameraPosition, cameraTarget), kMinOrbitDistance);

  // Flags to track if the camera's position, target, up vector, or orientation has changed during
  // user interaction. If any of these parameters change, the camera's transformation will be
  // updated accordingly.
  bool cameraChanged = false;
  bool orientationChanged = false;

  // Adjust the style and font scale for the camera section to make it more compact and visually
  // appealing. The frame padding is reduced, and the font scale is set to 0.92 for better
  // readability of the camera controls.
  ::ImGui::PushStyleVar(
      ImGuiStyleVar_FramePadding,
      ImVec2(::ImGui::GetStyle().FramePadding.x, ::ImGui::GetStyle().FramePadding.y - 1.0F));
  ::ImGui::SetWindowFontScale(0.90F);

  // Render the section title for the camera controls, including an icon and a brief description
  // of the camera parameters that can be adjusted in this section.
  renderSectionTitle(ICON_FA_VIDEO " Camera", "Position, aim, and projection in a compact panel.");

  // Set up sliders for camera position, target, and up vector, allowing users to adjust these
  // parameters.
  cameraChanged |= renderVec3Editor("Position", cameraPosition, 0.0F, kPositionDragSpeed, "%.2f");
  cameraChanged |= renderVec3Editor("Target", cameraTarget, 0.0F, kPositionDragSpeed, "%.2f");
  cameraChanged |= renderVec3Editor("Up", cameraUp, 0.0F, kPositionDragSpeed, "%.2f");

  // Set up sliders for camera orientation (yaw and pitch), allowing users to adjust the camera's
  // rotation around its target point. The yaw angle is clamped between -360 and 360 degrees,
  // while the pitch angle is clamped between -89 and 89 degrees to prevent gimbal lock and
  // unnatural camera behavior.
  float yawDegrees = yawPitch.x;
  float pitchDegrees = yawPitch.y;
  orientationChanged |=
      renderFloatRow("Yaw", yawDegrees, kCameraRotateSpeed, -360.0F, 360.0F, "%.1f deg");
  orientationChanged |= renderFloatRow("Pitch", pitchDegrees, kCameraRotateSpeed,
                                       -kPitchLimitDegrees, kPitchLimitDegrees, "%.1f deg");

  // Update the yaw and pitch values based on user input, and clamp the pitch to prevent unnatural
  // camera behavior.
  yawPitch = glm::vec2(yawDegrees, pitchDegrees);

  /// If the orientation has changed, compute the new forward vector based on the
  // updated yaw and pitch, and update the camera's target position accordingly. The right and
  // up axes are also recalculated based on the new forward vector, and the camera's
  // transformation is marked as changed.
  if (orientationChanged)
  {
    // Compute new forward vector based on updated yaw and pitch, and update camera target
    // accordingly.
    yawPitch.y = std::clamp(yawPitch.y, -kPitchLimitDegrees, kPitchLimitDegrees);
    const glm::vec3 rotatedForward = makeForwardFromYawPitchDegrees(yawPitch.x, yawPitch.y);

    // Update camera target based on new forward vector and orbit distance.
    cameraTarget = cameraPosition + rotatedForward * orbitDistance;

    // Update right and up axes based on new forward vector
    rightAxis = computeCameraRight(rotatedForward, cameraUp);
    upAxis = computeCameraUp(rightAxis, rotatedForward);
    forwardAxis = rotatedForward;

    // Mark camera as changed to update its transformation.
    cameraChanged = true;
  }

  ::ImGui::Spacing();

  // Render the projection section, allowing users to adjust the camera's field of view (FOV),
  // near plane, and far plane. The section is collapsible, and the controls are displayed when
  // the section is expanded. The FOV is clamped between 1 and 179 degrees, while the near and far
  // planes are clamped to ensure valid values for the camera's projection matrix.
  if (::ImGui::CollapsingHeader(ICON_FA_ARROWS_TO_EYE " projection",
                                ImGuiTreeNodeFlags_DefaultOpen))
  {
    cameraChanged |= renderFloatRow("FOV", camera.m_fov, 0.25F, 1.0F, 179.0F, "%.1f deg");
    cameraChanged |= renderFloatRow("Near", camera.m_nearPlane, 0.01F, kMinNearPlane,
                                    camera.m_farPlane - kMinNearPlane, "%.3f");
    cameraChanged |= renderFloatRow("Far", camera.m_farPlane, 0.1F,
                                    camera.m_nearPlane + kMinNearPlane, 5000.0F, "%.1f");
  }

  // If any camera parameters have changed, update the camera's transformation with the new
  // values.
  if (cameraChanged)
  {
    camera.transform({.position = cameraPosition,
                      .center = cameraTarget,
                      .up = normalizeOrFallback(cameraUp, upAxis),
                      .fov = camera.m_fov,
                      .nearPlane = glm::max(camera.m_nearPlane, kMinNearPlane),
                      .farPlane = glm::max(camera.m_farPlane, camera.m_nearPlane + kMinNearPlane)});
  }

  // Restore the original style and font scale after rendering the camera section.
  ::ImGui::SetWindowFontScale(1.0F);
  ::ImGui::PopStyleVar();
}

// Render the lights section in the inspector, allowing users to adjust parameters for
// directional, point, and spot lights in the scene.
auto renderLightsSection(Scene &scene) -> void
{
  // Calculate the number of each type of light in the scene and the total number of lights. This
  // information is used to display a caption for the lights section, providing an overview of the
  // lighting setup in the scene.
  const auto numDirectionalLights = scene.getDirectionalLights().size();
  const auto numPointLights = scene.getPointLights().size();
  const auto numSpotLights = scene.getSpotLights().size();
  const auto totalLights = numDirectionalLights + numPointLights + numSpotLights;
  const std::string lightsCaption =
      "Scene light controls by type. Total lights: " + std::to_string(totalLights);

  // Render the section title for the lights section, including an icon and a caption that
  // summarizes the number of lights in the scene. This provides users with a quick overview of
  // the lighting setup and allows them to access controls for each type of light.
  renderSectionTitle(ICON_FA_LIGHTBULB " Lights", lightsCaption.c_str());

  // Render the controls for each type of light in the scene, allowing users to adjust parameters
  // such as direction, position, color, intensity, and cone angles. Each light type has its own
  // section with collapsible headers, providing a clear and organized interface for managing the
  // scene's lighting setup.
  renderDirectionalLightsSection(scene);
  renderPointLightsSection(scene);
  renderSpotLightsSection(scene);
}

// Render the inspector window, which includes the object list, camera controls, and light
// controls.
auto renderInspector(Scene &scene, std::optional<uint32_t> &selectedObjectId) -> void
{
  // Determine the size and position of the inspector window based on the display size and
  // margins.
  const ImVec2 displaySize = ::ImGui::GetIO().DisplaySize;
  const float inspectorHeight = glm::max(320.0F, displaySize.y - (2.0F * kWindowScreenMargin));
  const float inspectorWidth =
      glm::min(kInspectorWidth, displaySize.x - (2.0F * kWindowScreenMargin));

  ::ImGui::SetNextWindowPos(ImVec2(kWindowScreenMargin, kWindowScreenMargin), ImGuiCond_Appearing);
  ::ImGui::SetNextWindowSize(ImVec2(inspectorWidth, inspectorHeight), ImGuiCond_Appearing);
  ::ImGui::SetNextWindowSizeConstraints(
      ImVec2(320.0F, 320.0F),
      ImVec2(displaySize.x - kWindowScreenMargin, displaySize.y - kWindowScreenMargin));

  // Begin the inspector window with a title and icon.
  ::ImGui::Begin(ICON_FA_SLIDERS " Inspector");

  // Render the list of objects in the scene, allowing selection of an object.
  renderObjectList(scene, selectedObjectId);
  // Add spacing between sections for visual clarity.
  ::ImGui::Dummy(ImVec2(0.0F, kSectionSpacing));
  // Render the camera controls, allowing manipulation of the camera's position, target, and
  // projection.
  renderCameraSection(scene);
  // Add spacing between sections for visual clarity.
  ::ImGui::Dummy(ImVec2(0.0F, kSectionSpacing));
  // Render the lights section, allowing manipulation of directional, point, and spot lights in
  // the scene.
  renderLightsSection(scene);

  // End the inspector window.
  ::ImGui::End();
}

// Setup the ImGui style for the editor, customizing the appearance of windows, frames, buttons,
// and text. This function modifies the ImGui style settings to create a visually appealing and
// consistent user interface for the editor, including rounded corners, padding, spacing, and
// color schemes for various UI elements.
void setupEditorStyle()
{
  ::ImGuiStyle &style = ::ImGui::GetStyle();

  // Window appearance
  style.WindowRounding = 8.0f;
  style.ChildRounding = 6.0f;
  style.FrameRounding = 5.0f;
  style.PopupRounding = 6.0f;
  style.ScrollbarRounding = 8.0f;
  style.GrabRounding = 5.0f;

  // Spacing
  style.WindowPadding = ImVec2(12, 12);
  style.FramePadding = ImVec2(8, 5);
  style.ItemSpacing = ImVec2(8, 6);
  style.ItemInnerSpacing = ImVec2(6, 4);

  // Borders
  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;

  // Colors
  ImVec4 *colors = style.Colors;

  colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.105f, 0.12f, 1.0f);

  colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.085f, 0.10f, 1.0f);

  colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.17f, 0.20f, 1.0f);

  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.24f, 0.28f, 1.0f);

  colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.28f, 0.35f, 1.0f);

  // Selection blue
  colors[ImGuiCol_Header] = ImVec4(0.18f, 0.38f, 0.65f, 0.8f);

  colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.48f, 0.85f, 0.8f);

  colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.42f, 0.75f, 1.0f);

  // Buttons
  colors[ImGuiCol_Button] = ImVec4(0.18f, 0.20f, 0.24f, 1.0f);

  colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.32f, 0.40f, 1.0f);

  colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.45f, 0.65f, 1.0f);

  // Text
  colors[ImGuiCol_Text] = ImVec4(0.90f, 0.92f, 0.95f, 1.0f);

  colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.52f, 0.55f, 1.0f);
}

} // namespace

///////////////////////////////////////////////////////
// ImGuiRenderer Implementation
///////////////////////////////////////////////////////

ImGuiRenderer::ImGuiRenderer(VulkanContext &context, Swapchain &swapchain, Scene &scene,
                             std::optional<uint32_t> framesInFlight)
    : m_context(context), m_swapchain(swapchain), m_scene(scene), m_framesInFlight(framesInFlight)
{
}

ImGuiRenderer::~ImGuiRenderer() { destroy(); }

void ImGuiRenderer::init()
{
  m_context.getWindow().getFramebufferSize(m_framebufferWidth, m_framebufferHeight);

  createDescriptorPool();

  initImGui();
}

void ImGuiRenderer::createDescriptorPool()
{
  std::array<vk::DescriptorPoolSize, 11> poolSizes = {{
      {.type = vk::DescriptorType::eSampler, .descriptorCount = 1000},
      {.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1000},
      {.type = vk::DescriptorType::eSampledImage, .descriptorCount = 1000},
      {.type = vk::DescriptorType::eStorageImage, .descriptorCount = 1000},
      {.type = vk::DescriptorType::eUniformTexelBuffer, .descriptorCount = 1000},
      {.type = vk::DescriptorType::eStorageTexelBuffer, .descriptorCount = 1000},
      {.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1000},
      {.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 1000},
      {.type = vk::DescriptorType::eUniformBufferDynamic, .descriptorCount = 1000},
      {.type = vk::DescriptorType::eStorageBufferDynamic, .descriptorCount = 1000},
      {.type = vk::DescriptorType::eInputAttachment, .descriptorCount = 1000},
  }};

  vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = 1000 * static_cast<uint32_t>(poolSizes.size()),
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data(),
  };

  m_descriptorPool = vk::raii::DescriptorPool(m_context.getDevice().logical(), poolInfo);
}

void ImGuiRenderer::initImGui()
{
  // Create ImGui context
  auto imGuiVersionCheck = ::IMGUI_CHECKVERSION();
  if (!imGuiVersionCheck)
  {
    spdlog::error("ImGui version mismatch detected. Please ensure that the correct version of "
                  "ImGui is linked.");
    std::abort();
  }
  ::ImGui::CreateContext();

  ::ImGuiIO &guiIO = ::ImGui::GetIO();
  guiIO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;    // Enable Keyboard Controls
  guiIO.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos; // Enable Docking
  const auto swapExtent = m_swapchain.getExtent();
  guiIO.DisplaySize.x = static_cast<float>(swapExtent.width);
  guiIO.DisplaySize.y = static_cast<float>(swapExtent.height);

  ::ImGui::GetStyle().FontScaleMain = 1.0f; // Set default font scale
  ::ImGui::StyleColorsDark();               // Set ImGui style to dark

  // Load normal text font
  ::ImFontConfig fontConfig;

  ::ImFont *mainFont = guiIO.Fonts->AddFontFromFileTTF(
      PROJECT_SOURCE_DIR "/assets/fonts/Roboto-Regular.ttf", 18.0f, &fontConfig);

  if (mainFont == nullptr)
  {
    spdlog::error("Failed to load main font from file: assets/fonts/Roboto-Regular.ttf, use "
                  "default font instead.");
    mainFont = guiIO.Fonts->AddFontDefault();
  }

  // Merge Font Awesome icons into Roboto
  ::ImFontConfig iconConfig;
  iconConfig.MergeMode = true;
  iconConfig.PixelSnapH = true;

  std::vector<::ImWchar> iconRanges = {ICON_MIN_FA, ICON_MAX_16_FA, 0};

  guiIO.Fonts->AddFontFromFileTTF(PROJECT_SOURCE_DIR "/assets/fonts/fa-solid-900.otf", 18.0f,
                                  &iconConfig, iconRanges.data());

  // Use Roboto as the default font
  guiIO.FontDefault = mainFont;

  setupEditorStyle();

  // Initialize ImGui for GLFW and Vulkan
  auto *window = m_context.getWindow().getGLFWwindow();
  if (window == nullptr)
  {
    spdlog::error(
        "GLFW window is null. Ensure that the window is created before initializing ImGui.");
    std::abort();
  }

  auto resultGlfwInit = ::ImGui_ImplGlfw_InitForVulkan(window, true);
  if (!resultGlfwInit)
  {
    spdlog::error("Failed to initialize ImGui for GLFW with Vulkan backend");
    return;
  }

  vk::Format colorFormat = m_swapchain.getSurfaceFormat().format;

  // Setup Vulkan initialization info for ImGui
  ImGui_ImplVulkan_InitInfo initInfo{};
  initInfo.ApiVersion = VK_API_VERSION_1_3;
  initInfo.Instance = *m_context.getInstance();
  initInfo.PhysicalDevice = *m_context.getDevice().physical();
  initInfo.Device = *m_context.getDevice().logical();
  initInfo.QueueFamily = m_context.getDefaultGraphicsQueue().familyIndex;
  initInfo.Queue = *m_context.getDefaultGraphicsQueue().vkQueue;
  initInfo.DescriptorPool = *m_descriptorPool;
  // If the number of frames in flight exceeds the number of swapchain images,
  // it may cause issues when only creating as many buffers as swapchain images.
  // This would mean we would reuse buffers in recording command buffers, while still in use by
  // other command buffers, which could lead to synchronization issues.
  // NOTE: This is not optimal, maybe just create as many images as frames in flight.
  if (m_framesInFlight.has_value())
  {
    initInfo.ImageCount = *m_framesInFlight;
    initInfo.MinImageCount = *m_framesInFlight;
  }
  else
  {
    initInfo.ImageCount = m_swapchain.getImages().size();
    initInfo.MinImageCount = m_swapchain.getMinImageCount();
  }
  initInfo.PipelineInfoMain.RenderPass = nullptr; // Will be set later during rendering
  initInfo.PipelineInfoMain.Subpass = 0;
  initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  initInfo.UseDynamicRendering = VK_TRUE;
  auto pipelineCreateInfo = vk::PipelineRenderingCreateInfo{
      .colorAttachmentCount = 1, .pColorAttachmentFormats = &colorFormat};
  initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineCreateInfo;

  auto resultVulkanInit = ImGui_ImplVulkan_Init(&initInfo);
  if (!resultVulkanInit)
  {
    spdlog::error("Failed to initialize ImGui with Vulkan backend");
    return;
  }

  spdlog::info("ImGui initialized successfully with Vulkan backend");
}

auto ImGuiRenderer::recreateWithSwapchain() -> void
{
  const auto swapExtent = m_swapchain.getExtent();
  m_framebufferWidth = static_cast<int>(swapExtent.width);
  m_framebufferHeight = static_cast<int>(swapExtent.height);

  ::ImGuiIO &guiIO = ::ImGui::GetIO();
  guiIO.DisplaySize =
      ImVec2(static_cast<float>(swapExtent.width), static_cast<float>(swapExtent.height));

  const auto swapImageCount = static_cast<uint32_t>(m_swapchain.getImages().size());
  ImGui_ImplVulkan_SetMinImageCount(swapImageCount);
}

void ImGuiRenderer::destroy()
{
  // Wait for the device to be idle before destroying ImGui resources and the descriptor pool
  m_context.getDevice().logical().waitIdle();

  // Cleanup ImGui resources
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ::ImGui::DestroyContext();

  // Cleanup Vulkan descriptor pool
  m_descriptorPool = nullptr;
}

auto ImGuiRenderer::recordCommandBuffer(vk::raii::CommandBuffer &commandBuffer, uint32_t imageIndex)
    -> void
{
  // No Begin CommandBuffer call here, as the command buffer is already begun before this function
  // is called. It is assumed that we record ImGui rendering commands after the main scene rendering
  // commands, so the command buffer is already in the recording state.

  // Get the swapchain extent for setting up the render area for ImGui rendering
  const auto swapExtent = m_swapchain.getExtent();

  // Setup render pass info for ImGui rendering
  vk::RenderingAttachmentInfo colorAttachmentInfo{
      .imageView = m_swapchain.getImageViews()[imageIndex],
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eLoad,
      .storeOp = vk::AttachmentStoreOp::eStore};

  vk::RenderingInfo renderingInfo{
      .renderArea = vk::Rect2D{.offset = {.x = 0, .y = 0},
                               .extent = {.width = swapExtent.width, .height = swapExtent.height}},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachmentInfo};

  // Begin dynamic rendering for ImGui
  commandBuffer.beginRendering(renderingInfo);

  // Render ImGui draw data
  ::ImDrawData *drawData = ::ImGui::GetDrawData();
  assert(drawData != nullptr &&
         "ImGui draw data is null. Ensure ImGui::Render() is called before this function.");
  ::ImGui_ImplVulkan_RenderDrawData(drawData, *commandBuffer);

  // End dynamic rendering
  commandBuffer.endRendering();

  // No End CommandBuffer call here, as the command buffer will be ended after this function is
  // called. The final swapchain image transition will also be handled by the main command buffer
  // recording.
}

auto ImGuiRenderer::update() -> void
{
  ::ImGuiIO &guiIo = ::ImGui::GetIO();
  ::ImGui_ImplVulkan_NewFrame();
  ::ImGui_ImplGlfw_NewFrame();
  ::ImGui::NewFrame();
  ImGuizmo::BeginFrame();

  // Render the inspector window, which includes the object list, camera controls, and light
  // controls.
  renderInspector(m_scene, m_selectedObjectId);
  // Render the transform window for the selected object, allowing manipulation of its position,
  // rotation, and scale.
  renderSelectedObjectTransformWindow(m_scene, m_selectedObjectId);
  // Handle mouse input for scene camera navigation, allowing users to orbit, pan, and zoom the
  // camera in the scene using mouse interactions.
  handleMouseSceneCameraNavigation(m_scene);

  ::ImGui::Render();
}

} // namespace vksim::ImGui