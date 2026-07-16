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
#include "vksim/core/scene/SceneObject.hpp"

namespace vksim::physics
{

/**
 * @brief Base for LBM configuration. Defines the number of dimensions and discrete velocity
 * directions (Q) as well as the discrete velocity vectors (c) and their corresponding weights (w).
 */
struct LBMConfig
{
};

struct D3Q15 : public LBMConfig
{
  uint32_t Dims = 3;
  uint32_t Q = 15;

  std::vector<glm::vec3> c = {{{0.0F, 0.0F, 0.0F},
                               {1.0F, 0.0F, 0.0F},
                               {-1.0F, 0.0F, 0.0F},
                               {0.0F, 1.0F, 0.0F},
                               {0.0F, -1.0F, 0.0F},
                               {0.0F, 0.0F, 1.0F},
                               {0.0F, 0.0F, -1.0F},
                               {1.0F, 1.0F, 1.0F},
                               {-1.0F, -1.0F, -1.0F},
                               {1.0F, -1.0F, 1.0F},
                               {-1.0F, 1.0F, -1.0F},
                               {1.0F, 1.0F, -1.0F},
                               {-1.0F, -1.0F, 1.0F},
                               {1.0F, -1.0F, -1.0F},
                               {-1.0F, 1.0F, 1.0F}}};

  std::vector<float> w = {2.0F / 9.0F,  1.0F / 9.0F,  1.0F / 9.0F,  1.0F / 9.0F,  1.0F / 9.0F,
                          1.0F / 9.0F,  1.0F / 9.0F,  1.0F / 72.0F, 1.0F / 72.0F, 1.0F / 72.0F,
                          1.0F / 72.0F, 1.0F / 72.0F, 1.0F / 72.0F, 1.0F / 72.0F, 1.0F / 72.0F};
};

struct D3Q19 : public LBMConfig
{
  uint32_t Dims = 3;
  uint32_t Q = 19;

  float cs = std::numbers::inv_sqrt3_v<float>;

  std::vector<glm::vec3> c = {{{0.0F, 0.0F, 0.0F},
                               {1.0F, 0.0F, 0.0F},
                               {-1.0F, 0.0F, 0.0F},
                               {0.0F, 1.0F, 0.0F},
                               {0.0F, -1.0F, 0.0F},
                               {0.0F, 0.0F, 1.0F},
                               {0.0F, 0.0F, -1.0F},
                               {1.0F, 1.0F, 1.0F},
                               {-1.0F, -1.0F, -1.0F},
                               {1.0F, -1.0F, 1.0F},
                               {-1.0F, 1.0F, -1.0F},
                               {1.0F, 1.0F, -1.0F},
                               {-1.0F, -1.0F, 1.0F},
                               {1.0F, -1.0F, -1.0F},
                               {-1.0F, 1.0F, 1.0F},
                               {2.5f / cs, 2.5f / cs, 2.5f / cs},
                               {-2.5f / cs, -2.5f / cs, -2.5f / cs},
                               {2.5f / cs, -2.5f / cs, 2.5f / cs},
                               {-2.5f / cs, 2.5f / cs, -2.5f / cs}}};

  std::vector<float> w = {1.0F / 3.0F,  1.0F / 18.0F, 1.0F / 18.0F, 1.0F / 18.0F, 1.0F / 18.0F,
                          1.0F / 18.0F, 1.0F / 18.0F, 1.0F / 36.0F, 1.0F / 36.0F, 1.0F / 36.0F,
                          1.0F / 36.0F, 1.0F / 36.0F, 1.0F / 36.0F, 1.0F / 36.0F, 1.0F / 36.0F,
                          1.0F / 72.0F, 1.0F / 72.0F, 1.0F / 72.0F, 1.0F / 72.0F};
};

/** @brief Structure to hold information for creating a LBM fluid simulation.
 */
struct LBMFluidInfo
{
  glm::f32 tau{0.8f};                   // Relaxation time for the LBM simulation
  glm::f32 rho{1.0f};                   // Fluid density
  glm::f32 cellSize{1.0f};              // Cell size for the LBM simulation
  std::pair<glm::vec3, glm::vec3> aabb; // Axis-aligned bounding box for the fluid domain
  uint32_t currentBufferIndex{0};       // Index of the current buffer for double buffering
};

/** @brief Base class for LBM fluid simulation, providing a common interface for different
 *        configurations.
 */
class LBMFluidBase
{
public:
  virtual ~LBMFluidBase() = default;
  virtual auto init(LBMFluidInfo info) -> void = 0;

  [[nodiscard]] virtual auto getFluidInfo() const -> LBMFluidInfo = 0;
  [[nodiscard]] virtual auto getNumCells() const -> glm::vec3 = 0;

  virtual auto recordCommandBuffer(vk::raii::CommandBuffer &commandBuffer) -> void = 0;
};

template <typename T>
concept LatticeConfig = std::derived_from<T, LBMConfig>;

template <typename T>
concept Floating = std::is_floating_point_v<T>;

/** @brief Lattice Boltzmann Method (LBM) fluid simulation class template.
 * @tparam Dims The number of spatial dimensions (2 or 3).
 * @tparam Q The number of discrete velocity directions.
 * @tparam T The data type for the fluid properties (default: glm::f32).
 */
template <LatticeConfig T, Floating U = glm::f32> class LBMFluid : public LBMFluidBase
{
public:
  using type = T;

  LBMFluid(VulkanContext &vulkanContext) : m_vulkanContext(vulkanContext) {};
  LBMFluid(const LBMFluid &) = delete;
  LBMFluid(LBMFluid &&) noexcept = default;

  auto operator=(const LBMFluid &) -> LBMFluid & = delete;
  auto operator=(LBMFluid &&) -> LBMFluid & = delete;

  /**
   * @brief Initializes the LBM fluid simulation with the provided parameters.
   * @param info The LBMFluidInfo structure containing simulation parameters.
   */
  auto init(LBMFluidInfo info) -> void override
  {
    m_tau = static_cast<U>(info.tau);
    m_rho = static_cast<U>(info.rho);
    m_cellSize = static_cast<U>(info.cellSize);
    m_aabb = static_cast<std::pair<glm::tvec3<U>, glm::tvec3<U>>>(info.aabb);

    // Compute the number of cells in each dimension based on the AABB and cell size
    glm::tvec3<U> min = m_aabb.first;
    glm::tvec3<U> max = m_aabb.second;
    glm::tvec3<U> size = max - min;
    glm::tvec3<U> numCells = glm::ceil(size / m_cellSize);
    U totalCells = numCells.x * numCells.y * numCells.z;

    // Create a buffer for LBM configuration parameters
    m_LBMConfigBuffer.emplace(m_vulkanContext.getDevice());
    m_LBMConfigBuffer->create(BufferCreateInfo{
        .size = sizeof(m_latticeConfig),
        .usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal});

    // Create buffers for fluid properties and distribution functions
    m_fluidInfoBuffer.emplace(m_vulkanContext.getDevice());
    m_fluidInfoBuffer->create(BufferCreateInfo{
        .size = sizeof(LBMFluidInfo),
        .usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal});

    m_fluidDistributionBuffer.emplace(m_vulkanContext.getDevice());
    m_fluidDistributionBuffer->create(BufferCreateInfo{
        .size = totalCells * m_latticeConfig.Q * sizeof(U) * 2, // Two buffers for double buffering
        .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal});

    // Initialize the fluid info buffer with the provided parameters
    // Allocate a command buffer from the command pool associated with the default transfer queue
    const auto &defaultQueue = m_vulkanContext.getDefaultTransferQueue();
    const auto &commandPool = m_vulkanContext.getCommandPool(defaultQueue.familyIndex);
    vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool.get(),
                                            .level = vk::CommandBufferLevel::ePrimary,
                                            .commandBufferCount = 1};
    auto commandBuffers =
        vk::raii::CommandBuffers(m_vulkanContext.getDevice().logical(), allocInfo);

    // Begin recording commands into the command buffer
    vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
    commandBuffers.front().begin(beginInfo);

    // Copy the LBM config buffers by using a staging buffer to transfer data from the host to the
    // device.
    {
      Buffer stagingBuffer(m_vulkanContext.getDevice());
      stagingBuffer.create(
          BufferCreateInfo{.size = sizeof(m_latticeConfig),
                           .usage = vk::BufferUsageFlagBits::eTransferSrc,
                           .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                         vk::MemoryPropertyFlagBits::eHostCoherent});
      // Map the staging buffer memory and copy the fluid info data
      auto *mappedMemory = stagingBuffer.getVkBufferMemory().mapMemory(0, sizeof(m_latticeConfig));
      std::memcpy(mappedMemory, &m_latticeConfig, sizeof(m_latticeConfig));
      stagingBuffer.getVkBufferMemory().unmapMemory();

      // Copy the staging buffer to the device buffer
      vk::BufferCopy copyRegion{.size = sizeof(m_latticeConfig)};
      m_LBMConfigBuffer->copyFromBuffer(stagingBuffer, sizeof(m_latticeConfig),
                                        commandBuffers.front());
    }

    // Copy the fluid info buffer by using a staging buffer to transfer data from the host to the
    // device.
    {
      Buffer stagingBuffer(m_vulkanContext.getDevice());
      stagingBuffer.create(
          BufferCreateInfo{.size = sizeof(LBMFluidInfo),
                           .usage = vk::BufferUsageFlagBits::eTransferSrc,
                           .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                         vk::MemoryPropertyFlagBits::eHostCoherent});
      // Map the staging buffer memory and copy the fluid info data
      auto *mappedMemory = stagingBuffer.getVkBufferMemory().mapMemory(0, sizeof(LBMFluidInfo));
      std::memcpy(mappedMemory, &info, sizeof(LBMFluidInfo));
      stagingBuffer.getVkBufferMemory().unmapMemory();

      // Copy the staging buffer to the device buffer
      vk::BufferCopy copyRegion{.size = sizeof(LBMFluidInfo)};
      m_fluidInfoBuffer->copyFromBuffer(stagingBuffer, sizeof(LBMFluidInfo),
                                        commandBuffers.front());
    }

    // Copy the fluid distribution buffer by using a staging buffer to transfer data from the host
    // to the device.
    {
      Buffer stagingBuffer(m_vulkanContext.getDevice());
      stagingBuffer.create(
          BufferCreateInfo{.size = totalCells * m_latticeConfig.Q * sizeof(U) *
                                   2, // Two buffers for double buffering
                           .usage = vk::BufferUsageFlagBits::eTransferSrc,
                           .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                         vk::MemoryPropertyFlagBits::eHostCoherent});
      // Map the staging buffer memory and initialize the fluid distribution data to zero
      auto *mappedMemory = stagingBuffer.getVkBufferMemory().mapMemory(
          0, totalCells * m_latticeConfig.Q * sizeof(U) * 2);
      std::memset(mappedMemory, 0, totalCells * m_latticeConfig.Q * sizeof(U) * 2);
      stagingBuffer.getVkBufferMemory().unmapMemory();

      // Copy the staging buffer to the device buffer
      vk::BufferCopy copyRegion{.size = totalCells * m_latticeConfig.Q * sizeof(U) * 2};
      m_fluidDistributionBuffer->copyFromBuffer(
          stagingBuffer, totalCells * m_latticeConfig.Q * sizeof(U) * 2, commandBuffers.front());
    }

    // End recording commands into the command buffer
    commandBuffers.front().end();

    // Submit the command buffer to the default transfer queue and wait for completion
    vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandBuffers.front()};
    defaultQueue.vkQueue.submit(submitInfo, nullptr);
    defaultQueue.vkQueue.waitIdle();
  }

  /**
   * @brief Returns a reference to the fluid info buffer.
   * @return A std::expected containing a reference to the fluid info buffer or an error message.
   */
  [[nodiscard]] auto getFluidInfoBuffer() const
      -> std::expected<std::reference_wrapper<const Buffer>, std::string>
  {
    if (!m_fluidInfoBuffer.has_value())
    {
      return std::unexpected("Fluid info buffer not initialized.");
    }
    return std::ref(m_fluidInfoBuffer.value());
  }

  /**
   * @brief Returns a reference to the fluid distribution buffer.
   * @return A std::expected containing a reference to the fluid distribution buffer or an error
   * message.
   */
  [[nodiscard]] auto getFluidDistributionBuffer() const
      -> std::expected<std::reference_wrapper<const Buffer>, std::string>
  {
    if (!m_fluidDistributionBuffer.has_value())
    {
      return std::unexpected("Fluid distribution buffer not initialized.");
    }
    return std::ref(m_fluidDistributionBuffer.value());
  }

  /**
   * @brief Returns the current fluid simulation parameters.
   * @return An LBMFluidInfo structure containing the current simulation parameters.
   */
  [[nodiscard]] auto getFluidInfo() const -> LBMFluidInfo override
  {
    return LBMFluidInfo{.tau = static_cast<glm::f32>(m_tau),
                        .rho = static_cast<glm::f32>(m_rho),
                        .cellSize = static_cast<glm::f32>(m_cellSize),
                        .aabb = static_cast<std::pair<glm::vec3, glm::vec3>>(m_aabb)};
  }

  /**
   * @brief Returns the number of cells in each dimension of the fluid simulation.
   * @return A glm::vec3 representing the number of cells in x, y, and z dimensions.
   */
  [[nodiscard]] auto getNumCells() const -> glm::vec3 override
  {
    glm::tvec3<U> min = m_aabb.first;
    glm::tvec3<U> max = m_aabb.second;
    glm::tvec3<U> size = max - min;
    return glm::ceil(size / m_cellSize);
  }

  /**
   * @brief Records commands into the provided command buffer for the LBM fluid simulation.
   * @param commandBuffer The command buffer to record commands into.
   */
  auto recordCommandBuffer(vk::raii::CommandBuffer &commandBuffer) -> void override
  {
    // Placeholder for recording commands related to the LBM fluid simulation.
    // This function should record commands to update the fluid distribution functions,
    // apply boundary conditions, and perform any necessary computations on the GPU.
  }

private:
  VulkanContext &m_vulkanContext;
  std::optional<Buffer> m_LBMConfigBuffer;         // Buffer to store LBM configuration parameters
  std::optional<Buffer> m_fluidInfoBuffer;         // Buffer to store fluid properties
  std::optional<Buffer> m_fluidDistributionBuffer; // Buffer to store fluid distribution functions

  T m_latticeConfig; // Lattice configuration (e.g., D3Q15, D3Q19)

  U m_tau{static_cast<U>(0.8)};                   // Relaxation time for the LBM simulation
  U m_rho{static_cast<U>(1.0)};                   // Fluid density
  U m_cellSize{static_cast<U>(1.0)};              // Cell size for the LBM simulation
  std::pair<glm::tvec3<U>, glm::tvec3<U>> m_aabb; // Axis-aligned bounding box for the fluid domain
};
} // namespace vksim::physics
