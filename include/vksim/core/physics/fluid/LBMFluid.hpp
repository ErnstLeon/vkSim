#pragma once

#include "vksim/core/physics/voxelizer/Voxelizer.hpp"
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/gtx/hash.hpp>
#include <numbers>

#include "vksim/core/buffers/Buffer.hpp"
#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/physics/fluid/LBMConfigs.hpp"
#include "vksim/core/physics/voxelizer/VoxelGrid.hpp"
#include "vksim/core/physics/voxelizer/Voxelizer.hpp"
#include "vksim/slang/SlangCompiler.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim::physics
{

/** @brief Structure to hold information for creating a fluid simulation.
 */
struct FluidInfo
{
  glm::f32 tau{0.8f}; // Relaxation time for the LBM simulation
  glm::f32 rho{1.0f}; // Fluid density
};

/** @brief Structure to hold information for creating a fluid simulation, including fluid and
 * voxelization information.
 */
struct FluidSimulationInfo
{
  FluidInfo fluidInfo;
  VoxelizationInfo voxelizationInfo;
};

/** @brief Base class for LBM fluid simulation, providing a common interface for different
 *        configurations.
 */
class LBMFluidBase
{
public:
  virtual ~LBMFluidBase() = default;
  virtual auto init(Voxelizer &voxelizer,
                    std::vector<std::reference_wrapper<SceneObject>> &sceneObjects) -> void = 0;
  virtual auto recordCommandBuffer(vk::raii::CommandBuffer &commandBuffer) -> void = 0;

  virtual auto getpositionBuffer() -> Buffer & = 0;
  virtual auto getnormalBuffer() -> Buffer & = 0;
};

/** @brief Lattice Boltzmann Method (LBM) fluid simulation class template.
 * @tparam T Lattice configuration type (D3Q15 or D3Q19).
 * @tparam U Floating-point type for simulation parameters (default: glm::f32).
 */
template <typename T, typename U = glm::f32>
  requires(std::is_same_v<T, D3Q15> || std::is_same_v<T, D3Q19>) && std::is_floating_point_v<U>
class LBMFluid : public LBMFluidBase
{
public:
  using type = T;

  LBMFluid(VulkanContext &context, FluidSimulationInfo info);
  LBMFluid(const LBMFluid &) = delete;
  LBMFluid(LBMFluid &&) noexcept = default;

  auto operator=(const LBMFluid &) -> LBMFluid & = delete;
  auto operator=(LBMFluid &&) -> LBMFluid & = delete;

  /**
   * @brief Initializes the LBM fluid simulation with the provided parameters.
   * @param voxelizer Reference to the Voxelizer for voxelization of the scene.
   * @param sceneObjects Reference to the vector of scene objects in the scene.
   */
  auto init(Voxelizer &voxelizer, std::vector<std::reference_wrapper<SceneObject>> &sceneObjects)
      -> void override;

  /**
   * @brief Creates Vulkan buffers for the LBM fluid simulation, including configuration, fluid
   * info, and distribution buffers.
   */
  auto createBuffers() -> void;

  /**
   * @brief Creates a descriptor pool for the LBM fluid simulation, allowing for the allocation of
   * descriptor sets.
   */
  auto createDescriptorPool() -> void;

  /**
   * @brief Creates a descriptor set layout for the LBM fluid simulation, defining the bindings
   * for uniform and storage buffers.
   */
  auto createDescriptorSetLayout() -> void;

  auto createDescriptorSets() -> void;

  auto createPipeline() -> void;

  /**
   * @brief Retrieves the position buffer used for storing vertex positions in the Marching Cubes
   * algorithm.
   * @return Reference to the position buffer.
   */
  auto getpositionBuffer() -> Buffer & override;

  /**
   * @brief Retrieves the normal buffer used for storing vertex normals in the Marching Cubes
   * algorithm.
   * @return Reference to the normal buffer.
   */
  auto getnormalBuffer() -> Buffer & override;

  /**
   * @brief Records commands into the provided command buffer for the LBM fluid simulation.
   * @param commandBuffer The command buffer to record commands into.
   */
  auto recordCommandBuffer(vk::raii::CommandBuffer &commandBuffer) -> void override;

private:
  VulkanContext &m_context;

  // Fluid owns the voxel grid, which is used for voxelization and Marching Cubes.
  std::optional<VoxelGrid> m_voxelgrid;

  std::optional<Buffer> m_LBMConfigBuffer;         // Buffer to store LBM configuration parameters
  std::optional<Buffer> m_fluidInfoBuffer;         // Buffer to store fluid properties
  std::optional<Buffer> m_fluidDistributionBuffer; // Buffer to store fluid distribution functions

  std::optional<Buffer> m_positionBuffer;    // Buffer to store mesh vertex positions
  std::optional<Buffer> m_normalBuffer;      // Buffer to store mesh vertex normals
  std::optional<Buffer> m_vertexCountBuffer; // Buffer to store the number of vertices

  T m_latticeConfig; // Lattice configuration (e.g., D3Q15, D3Q19)

  FluidSimulationInfo m_fluidSimulationInfo; // Fluid simulation parameters (tau, rho, etc.)

  vk::raii::DescriptorPool m_descriptorPool = nullptr; // Descriptor pool for LBM fluid resources
  vk::raii::DescriptorSetLayout m_descriptorSetLayout =
      nullptr; // Descriptor set layout for LBM fluid resources
  std::vector<vk::raii::DescriptorSet> m_descriptorSets; // Descriptor sets for LBM fluid resources
  vk::raii::PipelineLayout m_pipelineLayout = nullptr;   // Pipeline layout for the compute pipeline
  vk::raii::Pipeline m_pipeline = nullptr; // Compute pipeline for the LBM fluid simulation

  vk::raii::DescriptorSetLayout m_marchCubesDescriptorSetLayout =
      nullptr; // Descriptor set layout for the Marching Cubes algorithm
  std::vector<vk::raii::DescriptorSet>
      m_marchCubesDescriptorSet; // Descriptor set for the Marching Cubes algorithm
  vk::raii::PipelineLayout m_marchCubesPipelineLayout =
      nullptr; // Pipeline layout for the Marching Cubes algorithm
  vk::raii::Pipeline m_marchCubesPipeline =
      nullptr; // Compute pipeline for the Marching Cubes algorithm
};
} // namespace vksim::physics

#include "vksim/core/physics/fluid/LBMFluid.tpp"
