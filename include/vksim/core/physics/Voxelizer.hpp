#pragma once

#include <expected>
#include <functional>
#include <vector>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/gtx/hash.hpp>
#include <numbers>

#include "vksim/core/buffers/Buffer.hpp"
#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/scene/Scene.hpp"

namespace vksim::physics
{
/** @brief Voxelizer class encapsulates the functionality for voxelizing a 3D scene into a grid of
 * voxels.
 */
class Voxelizer
{
public:
  Voxelizer(VulkanContext &context, Scene &scene);

  /** @brief Constructs a Voxelizer with the specified scene and cell size.
   * @param cellSize The size of each voxel cell.
   */
  auto init(float cellSize) -> void;

  /** @brief Voxelizes the provided scene into a voxel grid.
   * @param scene The scene to voxelize.
   */
  [[nodiscard]] auto getNumCells() const -> glm::vec3;

  /** @brief Returns the cell size used for voxelization.
   * @return The cell size as a float.
   */
  [[nodiscard]] auto getCellSize() const -> float;

  /** @brief Returns the axis-aligned bounding box (AABB) of the voxelized scene.
   * @return A pair of glm::vec3 representing the minimum and maximum corners of the AABB.
   */
  [[nodiscard]] auto getAABB() const -> std::pair<glm::vec3, glm::vec3>;

  /** @brief Returns the buffer containing the voxel grid data.
   * @return A reference to the Buffer containing the voxel grid data.
   */
  [[nodiscard]] auto getVoxelGridBuffer() const
      -> std::expected<std::reference_wrapper<const Buffer>, std::string>;

  /** @brief Records commands into the provided command buffer for voxelization.
   * @param commandBuffer The command buffer to record commands into.
   */
  auto recordCommandBuffer(vk::raii::CommandBuffer &commandBuffer) -> void;

private:
  VulkanContext &m_context;
  Scene &m_scene;

  float m_cellSize;
  glm::vec3 m_numCells;
  std::pair<glm::vec3, glm::vec3> m_aabb;

  std::optional<Buffer> m_voxelGridBuffer; // Buffer to store voxel grid
};
} // namespace vksim::physics
