#pragma once

#include <expected>
#include <functional>
#include <optional>
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
#include "vksim/core/physics/voxelizer/VoxelGrid.hpp"
#include "vksim/core/scene/SceneObject.hpp"

namespace vksim::physics
{
/** @brief Voxelizer class encapsulates the functionality for voxelizing a 3D scene into a grid of
 * voxels. It provides methods to initialize the voxelizer, record commands for voxelization. The
 * voxel grid is owned by the Fluid simulation and is passed to the Voxelizer for voxelization. The
 * Voxelizer does not own the voxel grid but operates on it.
 */
class Voxelizer
{
public:
  Voxelizer(VulkanContext &context);

  /** @brief Initializes the Voxelizer with the specified scene and voxel grid.
   * @param voxelGrid The voxel grid to store the voxelized data.
   * @param sceneObjects References to the scene objects to be voxelized.
   */
  auto init(VoxelGrid &voxelGrid, std::vector<std::reference_wrapper<SceneObject>> &sceneObjects)
      -> void;

  /** @brief Records commands into the provided command buffer for voxelization.
   * @param commandBuffer The command buffer to record commands into.
   */
  auto recordCommandBuffer(vk::raii::CommandBuffer &commandBuffer) -> void;

private:
  /** @brief Creates a Vulkan descriptor pool for the voxelization resources.
   * @return A vk::raii::DescriptorPool object.
   */
  auto createDescriptorPool() -> void;

  /** @brief Creates a Vulkan descriptor set layout for the voxelization resources.
   * @return A vk::raii::DescriptorSetLayout object.
   */
  auto createDescriptorSetLayout() -> void;

  /** @brief Creates Vulkan descriptor sets for the voxelization resources.
   * @return A vector of vk::raii::DescriptorSet objects.
   */
  auto createDescriptorSets() -> void;

  /** @brief Creates a Vulkan compute pipeline for the voxelization process.
   * @return A vk::raii::Pipeline object.
   */
  auto createPipeline() -> void;

  VulkanContext &m_context;

  // Vector of scene objects to be voxelized. The Voxelizer does not own the scene objects but
  // operates on them.
  std::vector<std::reference_wrapper<SceneObject>> m_sceneObjects;

  std::optional<std::reference_wrapper<VoxelGrid>> m_voxelGrid;

  vk::raii::DescriptorPool m_descriptorPool{nullptr};
  vk::raii::DescriptorSetLayout m_descriptorSetLayout{nullptr};
  std::vector<vk::raii::DescriptorSet> m_descriptorSets;
  vk::raii::Pipeline m_pipeline{nullptr};
  vk::raii::PipelineLayout m_pipelineLayout{nullptr};
};
} // namespace vksim::physics
