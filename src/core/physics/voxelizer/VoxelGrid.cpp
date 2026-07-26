#include "vksim/core/physics/voxelizer/VoxelGrid.hpp"

#include <cmath>
#include <cstring>

#include "vksim/utility/Logging.hpp"

namespace vksim::physics
{

VoxelGrid::VoxelGrid(VulkanContext &context, VoxelizationInfo voxelizationInfo)
    : m_context(context), m_voxelizationInfo(std::move(voxelizationInfo))
{
  m_gridSize = glm::u32vec3{
      static_cast<uint32_t>(
          std::ceil((m_voxelizationInfo.aabb.second.x - m_voxelizationInfo.aabb.first.x) /
                    m_voxelizationInfo.cellSize)),
      static_cast<uint32_t>(
          std::ceil((m_voxelizationInfo.aabb.second.y - m_voxelizationInfo.aabb.first.y) /
                    m_voxelizationInfo.cellSize)),
      static_cast<uint32_t>(
          std::ceil((m_voxelizationInfo.aabb.second.z - m_voxelizationInfo.aabb.first.z) /
                    m_voxelizationInfo.cellSize))};

  m_totalCells = m_gridSize.x * m_gridSize.y * m_gridSize.z;
}

auto VoxelGrid::init() -> void { createBuffers(); }

auto VoxelGrid::getVoxelGridBuffer() -> Buffer & { return *m_voxelGridBuffer; }

auto VoxelGrid::getVoxelizationInfoBuffer() -> Buffer & { return *m_voxelizationInfoBuffer; }

[[nodiscard]] auto VoxelGrid::getCellSize() const -> float { return m_voxelizationInfo.cellSize; }

[[nodiscard]] auto VoxelGrid::getGridSize() const -> glm::u32vec3 { return m_gridSize; }

[[nodiscard]] auto VoxelGrid::getAABB() const -> std::pair<glm::vec3, glm::vec3>
{
  return m_voxelizationInfo.aabb;
}

[[nodiscard]] auto VoxelGrid::getTotalCells() const -> uint32_t { return m_totalCells; }

auto VoxelGrid::createBuffers() -> void
{
  // Create a buffer to store the voxel grid.
  // The buffer size is determined by the total number of cells in the voxel grid.
  auto totalCells = static_cast<glm::uint64_t>(m_gridSize.x) *
                    static_cast<glm::uint64_t>(m_gridSize.y) *
                    static_cast<glm::uint64_t>(m_gridSize.z);

  m_voxelGridBuffer.emplace(m_context);
  m_voxelGridBuffer->create(BufferCreateInfo{
      .size = totalCells *
              sizeof(uint8_t), // Assuming each voxel is represented by a single byte (0 or 1),
                               // bit would be more memory efficient but requires bit manipulation
                               // in the shader and worse access patterns, so we use a byte.
      .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
      .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
      .debugName = "VoxelGridBuffer"});

  // Create uniform buffer for voxelization parameters (cell size and AABB)
  // Vec4 for gridMin (xyz: min, w: cell size), gridMax (xyz: max, w: unused), and gridSize
  // (xyz: size, w: unused)
  uint32_t voxelGridParamsSize = (sizeof(glm::vec4) * 2) + (sizeof(uint32_t) * 4);
  m_voxelizationInfoBuffer.emplace(m_context);
  m_voxelizationInfoBuffer->create(BufferCreateInfo{
      .size = voxelGridParamsSize,
      .usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
      .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
      .debugName = "VoxelizationInfoBuffer"});

  // Copy voxelization parameters to the uniform buffer
  // NOTE: The layout of the data in the buffer must match the layout expected in the shader. So the
  // VoxelizationInfo struct is packed into a shader-friendly layout.
  m_voxelizationInfoBuffer->copyFromHost(&m_voxelizationInfo.aabb.first, sizeof(glm::vec3), 0, 0);
  m_voxelizationInfoBuffer->copyFromHost(&m_voxelizationInfo.cellSize, sizeof(float), 0,
                                         sizeof(glm::vec3));
  m_voxelizationInfoBuffer->copyFromHost(&m_voxelizationInfo.aabb.second, sizeof(glm::vec3), 0,
                                         sizeof(glm::vec4));
  m_voxelizationInfoBuffer->copyFromHost(&m_gridSize, 3 * sizeof(uint32_t), 0,
                                         (2 * sizeof(glm::vec4)));
  m_voxelizationInfoBuffer->copyFromHost(&m_totalCells, sizeof(uint32_t), 0,
                                         (2 * sizeof(glm::vec4)) + (3 * sizeof(uint32_t)));

  spdlog::info("Voxel grid buffer created with total cells: {}", m_totalCells);
}

} // namespace vksim::physics
