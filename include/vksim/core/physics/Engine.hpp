#pragma once

#include <memory>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/context/VulkanContext.hpp"
#include "vksim/core/physics/fluid/LBMFluid.hpp"
#include "vksim/core/physics/voxelizer/Voxelizer.hpp"
#include "vksim/core/scene/Scene.hpp"

namespace vksim::physics
{
/**
 * @brief Base class for physics engines, providing a common interface for different physics
 * simulations.
 */
class Engine
{
public:
  /**
   * @brief Constructs a physics engine with the specified Vulkan context and scene.
   * @param context The Vulkan context.
   * @param scene The scene to be simulated.
   */
  Engine(VulkanContext &context, Scene &scene);

  /**
   * @brief Evolves the physics simulation by one time step.
   */
  auto evolve() -> void;

  /**
   * @brief Voxelizes the scene for fluid simulation.
   */
  auto voxelize() -> void;

private:
  /** @brief Creates command buffers for the physics simulation. */
  auto createCommandBuffers() -> void;

  /** @brief Records commands into the command buffers for the physics simulation. */
  auto recordCommandBuffers() -> void;

  VulkanContext &m_context;
  Scene &m_scene;

  std::optional<vk::QueryPool> m_queryPool;

  // Command buffers for the physics simulation, including voxelization and LBM fluid simulation.
  std::vector<vk::raii::CommandBuffer> m_commandBuffers;

  std::optional<Voxelizer> m_voxelizer;
  std::unique_ptr<LBMFluidBase> m_lbmFluid;

}; // class Engine

} // namespace vksim::physics