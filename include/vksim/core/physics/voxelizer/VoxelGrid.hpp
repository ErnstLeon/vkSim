#pragma once

#include <optional>
#include <utility>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>

#include "vksim/core/buffers/Buffer.hpp"
#include "vksim/core/context/VulkanContext.hpp"

namespace vksim::physics
{
/** @brief Structure to hold information for creating a voxel grid for the fluid simulation.
 */
struct VoxelizationInfo
{
  std::pair<glm::vec3, glm::vec3> aabb; // Axis-aligned bounding box for the voxel grid
  glm::f32 cellSize{1.0f};              // Size of each voxel cell
};

/** @brief Class representing a voxel grid, including cell size, number of cells, and
 * axis-aligned bounding box (AABB). This is the result of the voxelization process and is used for
 * LBM fluid simulation.
 */
class VoxelGrid
{
public:
  VoxelGrid(VulkanContext &context, VoxelizationInfo voxelizationInfo);

  auto init() -> void;

  auto getVoxelGridBuffer() -> Buffer &;
  auto getVoxelizationParamsBuffer() -> Buffer &;

  [[nodiscard]] auto getCellSize() const -> float;
  [[nodiscard]] auto getGridSize() const -> glm::u32vec3;
  [[nodiscard]] auto getAABB() const -> std::pair<glm::vec3, glm::vec3>;

  [[nodiscard]] auto getTotalCells() const -> uint32_t;

private:
  auto createBuffers() -> void;

  VulkanContext &m_context;
  VoxelizationInfo m_voxelizationInfo;

  // Number of cells in the voxel grid (x, y, z)
  glm::u32vec3 m_gridSize{0, 0, 0};
  // Total number of cells in the voxel grid (gridSize.x * gridSize.y * gridSize.z)
  uint32_t m_totalCells{0};

  // Buffer to store voxel grid data. Each voxel is represented by a uint8_t value.
  std::optional<Buffer> m_voxelGridBuffer;
  // Buffer to store voxelization parameters (cell size and AABB).
  std::optional<Buffer> m_voxelizationParamsBuffer;
};

} // namespace vksim::physics
