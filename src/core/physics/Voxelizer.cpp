#include "vksim/core/physics/Voxelizer.hpp"

namespace vksim::physics
{
Voxelizer::Voxelizer(VulkanContext &context, Scene &scene) : m_context(context), m_scene(scene) {}

auto Voxelizer::init(float cellSize) -> void
{
  m_cellSize = cellSize;

  // Compute the axis-aligned bounding box (AABB) of the scene.
  m_aabb = m_scene.getAABB();

  // Compute the number of cells in each dimension based on the AABB and cell size.
  glm::vec3 size = m_aabb.second - m_aabb.first;
  m_numCells = glm::ceil(size / m_cellSize);

  // Create a buffer to store the voxel grid.
  // The buffer size is determined by the total number of cells in the voxel grid.
  auto totalCells = static_cast<size_t>(m_numCells.x * m_numCells.y * m_numCells.z);
  m_voxelGridBuffer.emplace(m_context.getDevice());
  m_voxelGridBuffer->create(BufferCreateInfo{
      .size = totalCells * sizeof(uint8_t), // Assuming each voxel is represented by a uint8_t
      .usage = vk::BufferUsageFlagBits::eStorageBuffer,
      .properties = vk::MemoryPropertyFlagBits::eDeviceLocal});
}

auto Voxelizer::getNumCells() const -> glm::vec3 { return m_numCells; }

auto Voxelizer::getCellSize() const -> float { return m_cellSize; }

auto Voxelizer::getAABB() const -> std::pair<glm::vec3, glm::vec3> { return m_aabb; }

auto Voxelizer::getVoxelGridBuffer() const
    -> std::expected<std::reference_wrapper<const Buffer>, std::string>
{
  if (m_voxelGridBuffer)
  {
    return std::cref(*m_voxelGridBuffer);
  }
  return std::unexpected<std::string>("Voxel grid buffer is not initialized.");
}

auto Voxelizer::recordCommandBuffer(vk::raii::CommandBuffer &commandBuffer) -> void
{
  // Placeholder for recording commands related to voxelization.
  // This function should record commands to update the voxel grid on the GPU.
}

} // namespace vksim::physics
