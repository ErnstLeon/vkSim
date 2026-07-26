#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/gtx/hash.hpp>

#include "vksim/core/physics/Engine.hpp"
#include "vksim/core/physics/fluid/LBMFluid.hpp"
#include "vksim/core/physics/voxelizer/Voxelizer.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim::physics
{

Engine::Engine(VulkanContext &context, Scene &scene) : m_context(context), m_scene(scene)
{
  auto *fluidObject = m_scene.getFluid();

  if (fluidObject != nullptr) // Check if the scene has fluid simulation objects
  {
    // Create Vector with objects to be voxelized.
    std::vector<std::reference_wrapper<SceneObject>> sceneObjects;
    for (auto &object : m_scene.getObjects())
    {
      sceneObjects.emplace_back(*object);
    }

    // Initialize the voxelizer for the scene
    m_voxelizer.emplace(m_context);

    // Initialize the fluid simulation object with the voxelizer
    fluidObject->init(m_voxelizer.value(), sceneObjects);
  }

  createCommandBuffers();
  recordCommandBuffers();

  spdlog::info("Physics engine initialized with voxelizer and LBM fluid simulation.");
}

auto Engine::evolve() -> void
{
  // Submit the command buffer to the compute queue for execution
  const auto &computeQueue = m_context.getDefaultComputeQueue();
  vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*m_commandBuffers.back()};
  computeQueue.vkQueue.submit(submitInfo, nullptr);
}

auto Engine::voxelize() -> void
{ // Submit the command buffer to the compute queue for execution
  const auto &computeQueue = m_context.getDefaultComputeQueue();
  vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*m_commandBuffers.front()};
  computeQueue.vkQueue.submit(submitInfo, nullptr);
  computeQueue.vkQueue.waitIdle(); // Wait for the voxelization to complete before proceeding
}

auto Engine::createCommandBuffers() -> void
{
  m_commandBuffers.clear();

  const auto &commandPool =
      m_context.getCommandPool(m_context.getDefaultComputeQueue().familyIndex);

  // Allocate two command buffers: one for voxelization and one for LBM fluid simulation
  m_commandBuffers =
      commandPool.allocateCommandBuffers({.level = vk::CommandBufferLevel::ePrimary, .count = 2});
}

auto Engine::recordCommandBuffers() -> void
{
  auto *fluidObject = m_scene.getFluid();
  if (fluidObject == nullptr)
  {
    spdlog::warn(
        "No fluid simulation object found in the scene. Skipping command buffer recording.");
    return;
  }

  // Record commands for voxelization
  {
    auto &voxelCommandBuffer = m_commandBuffers.front();
    vk::CommandBufferBeginInfo beginInfo{};
    voxelCommandBuffer.begin(beginInfo);
    m_voxelizer->recordCommandBuffer(voxelCommandBuffer);
    voxelCommandBuffer.end();
  }

  // Record commands for the physics simulation into the command buffer
  {
    auto &commandBuffer = m_commandBuffers.back();
    vk::CommandBufferBeginInfo beginInfo{};
    commandBuffer.begin(beginInfo);
    fluidObject->recordCommandBuffer(commandBuffer);
    commandBuffer.end();
  }

  spdlog::info("Physics engine command buffer recorded for voxelization and LBM fluid simulation.");
}
} // namespace vksim::physics
