#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace vksim::physics
{

template <typename T, typename U>
  requires(std::is_same_v<T, D3Q15> || std::is_same_v<T, D3Q19>) && std::is_floating_point_v<U>
LBMFluid<T, U>::LBMFluid(VulkanContext &context, FluidSimulationInfo info)
    : m_context(context), m_fluidSimulationInfo(std::move(info))
{
}

template <typename T, typename U>
  requires(std::is_same_v<T, D3Q15> || std::is_same_v<T, D3Q19>) && std::is_floating_point_v<U>
auto LBMFluid<T, U>::init(Voxelizer &voxelizer,
                          std::vector<std::reference_wrapper<SceneObject>> &sceneObjects) -> void
{
  // Initialize the voxel grid for the fluid simulation. The voxel grid is owned by the fluid
  // simulation and is used for voxelization and Marching Cubes.
  m_voxelgrid.emplace(m_context, m_fluidSimulationInfo.voxelizationInfo);
  m_voxelgrid->init();

  // Initialize the voxelizer with the scene and voxel grid.
  voxelizer.init(*m_voxelgrid, sceneObjects);

  // Create Vulkan buffers for the LBM fluid simulation.
  createBuffers();

  // Create the descriptor pool, descriptor set layout, descriptor sets, and compute pipeline for
  // voxelization.
  createDescriptorPool();
  createDescriptorSetLayout();
  createDescriptorSets();
  createPipeline();

  spdlog::info("LBMFluid initialized");
}

template <typename T, typename U>
  requires(std::is_same_v<T, D3Q15> || std::is_same_v<T, D3Q19>) && std::is_floating_point_v<U>
auto LBMFluid<T, U>::createBuffers() -> void
{
  auto totalCells = m_voxelgrid->getTotalCells();

  // Create a buffer for LBM configuration parameters (lattice configuration).
  m_LBMConfigBuffer.emplace(m_context);
  m_LBMConfigBuffer->create(BufferCreateInfo{.size = sizeof(m_latticeConfig),
                                             .usage = vk::BufferUsageFlagBits::eUniformBuffer |
                                                      vk::BufferUsageFlagBits::eTransferDst,
                                             .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                                             .debugName = "LBMConfigBuffer"});
  m_LBMConfigBuffer->copyFromHost(&m_latticeConfig, sizeof(m_latticeConfig));

  // Create buffer for fluid properties (voxelgrid tau, rho, etc.).
  m_fluidInfoBuffer.emplace(m_context);
  m_fluidInfoBuffer->create(BufferCreateInfo{.size = sizeof(FluidInfo),
                                             .usage = vk::BufferUsageFlagBits::eUniformBuffer |
                                                      vk::BufferUsageFlagBits::eTransferDst,
                                             .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                                             .debugName = "LBMFluidInfoBuffer"});
  m_fluidInfoBuffer->copyFromHost(&m_fluidSimulationInfo.fluidInfo, sizeof(FluidInfo));

  // Create a buffer for fluid distribution functions for each cell in the voxel grid.
  m_fluidDistributionBuffer.emplace(m_context);
  m_fluidDistributionBuffer->create(BufferCreateInfo{
      .size = static_cast<size_t>(totalCells * m_latticeConfig.Q * sizeof(U) * 2),
      .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
      .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
      .debugName = "LBMFluidDistributionBuffer"});

  // Create buffers for the Marching Cubes output.
  m_vertexCountBuffer.emplace(m_context);
  m_vertexCountBuffer->create(BufferCreateInfo{
      .size = sizeof(uint32_t),
      .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst |
               vk::BufferUsageFlagBits::eTransferSrc,
      .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
      .debugName = "MCubesVertexCountBuffer"});

  m_positionBuffer.emplace(m_context);
  m_positionBuffer->create(BufferCreateInfo{
      .size = static_cast<size_t>(totalCells * 15 * sizeof(glm::vec4)),
      .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer,
      .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
      .debugName = "MCubesPositionBuffer"});

  m_normalBuffer.emplace(m_context);
  m_normalBuffer->create(BufferCreateInfo{
      .size = static_cast<size_t>(totalCells * 15 * sizeof(glm::vec4)),
      .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer,
      .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
      .debugName = "MCubesNormalBuffer"});
}

template <typename T, typename U>
  requires(std::is_same_v<T, D3Q15> || std::is_same_v<T, D3Q19>) && std::is_floating_point_v<U>
auto LBMFluid<T, U>::createDescriptorPool() -> void
{
  // LBM + Marching Cubes resources in one pool.
  std::array<vk::DescriptorPoolSize, 2> poolSize;
  poolSize[0] =
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = 5};
  poolSize[1] =
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 5};

  vk::DescriptorPoolCreateInfo poolCreateInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = 2,
      .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
      .pPoolSizes = poolSize.data()};

  m_descriptorPool = vk::raii::DescriptorPool(m_context.getDevice().logical(), poolCreateInfo);

  spdlog::info(
      "Descriptor pool created for LBM fluid with {} uniform buffer(s) and {} storage buffer(s)",
      poolSize[0].descriptorCount, poolSize[1].descriptorCount);
}

template <typename T, typename U>
  requires(std::is_same_v<T, D3Q15> || std::is_same_v<T, D3Q19>) && std::is_floating_point_v<U>
auto LBMFluid<T, U>::createDescriptorSetLayout() -> void
{
  // Create descriptor set layout for LBM fluid simulation, including uniform and storage buffers.
  // Bindings for LBM simulation: 0 - LBMConfigBuffer, 1 - FluidInfoBuffer, 2 -
  // VoxelizationInfoBuffer, 3 - FluidDistributionBuffer
  {
    std::array<vk::DescriptorSetLayoutBinding, 4> bindings = {{
        {.binding = 0,
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
        {.binding = 1,
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
        {.binding = 2,
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
        {.binding = 3,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
    }};

    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{
        .bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data()};

    m_lbmDescriptorSetLayout =
        vk::raii::DescriptorSetLayout(m_context.getDevice().logical(), layoutCreateInfo);

    spdlog::info("Descriptor set layout created for LBM solver with {} bindings", bindings.size());
  }

  // Create descriptor set layout for Marching Cubes, including uniform and storage buffers.
  // Bindings for Marching Cubes: 0 - FluidInfoBuffer, 1 - VoxelizationInfoBuffer, 2 -
  // VoxelGridBuffer, 3 - PositionBuffer, 4 - NormalBuffer, 5 - VertexCountBuffer
  {
    std::array<vk::DescriptorSetLayoutBinding, 6> bindings = {{
        {.binding = 0,
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
        {.binding = 1,
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
        {.binding = 2,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
        {.binding = 3,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
        {.binding = 4,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
        {.binding = 5,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
    }};

    vk::DescriptorSetLayoutCreateInfo marchCubesLayoutCreateInfo{
        .bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data()};

    m_marchCubesDescriptorSetLayout =
        vk::raii::DescriptorSetLayout(m_context.getDevice().logical(), marchCubesLayoutCreateInfo);

    spdlog::info("Descriptor set layout created for Marching Cubes with {} bindings",
                 bindings.size());
  }
}

template <typename T, typename U>
  requires(std::is_same_v<T, D3Q15> || std::is_same_v<T, D3Q19>) && std::is_floating_point_v<U>
auto LBMFluid<T, U>::createDescriptorSets() -> void
{
  // Allocate descriptor sets for LBM fluid simulation
  {
    vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = *m_descriptorPool,
                                            .descriptorSetCount = 1,
                                            .pSetLayouts = &*m_lbmDescriptorSetLayout};
    m_lbmDescriptorSets = m_context.getDevice().logical().allocateDescriptorSets(allocInfo);

    // Update descriptor sets with buffer information for LBM Configuration (e.g., D3Q15)
    vk::DescriptorBufferInfo lbmConfigBufferInfo{
        .buffer = m_LBMConfigBuffer->getVkBuffer(), .offset = 0, .range = sizeof(m_latticeConfig)};
    vk::WriteDescriptorSet lbmConfigDescriptorWrites{.dstSet = m_lbmDescriptorSets[0],
                                                     .dstBinding = 0,
                                                     .dstArrayElement = 0,
                                                     .descriptorCount = 1,
                                                     .descriptorType =
                                                         vk::DescriptorType::eUniformBuffer,
                                                     .pBufferInfo = &lbmConfigBufferInfo};

    // Update descriptor sets with buffer information for Fluid Info (e.g., tau,
    // rho)
    vk::DescriptorBufferInfo fluidInfoBufferInfo{.buffer = m_fluidInfoBuffer->getVkBuffer(),
                                                 .offset = 0,
                                                 .range = m_fluidInfoBuffer->getSize()};
    vk::WriteDescriptorSet fluidInfoDescriptorWrites{.dstSet = m_lbmDescriptorSets[0],
                                                     .dstBinding = 1,
                                                     .dstArrayElement = 0,
                                                     .descriptorCount = 1,
                                                     .descriptorType =
                                                         vk::DescriptorType::eUniformBuffer,
                                                     .pBufferInfo = &fluidInfoBufferInfo};

    // Update descriptor sets with buffer information for Voxelization Info (e.g., aabb, cell size)
    vk::DescriptorBufferInfo voxelizationInfoBufferInfo{
        .buffer = m_voxelgrid->getVoxelizationInfoBuffer().getVkBuffer(),
        .offset = 0,
        .range = m_voxelgrid->getVoxelizationInfoBuffer().getSize()};
    vk::WriteDescriptorSet voxelizationInfoDescriptorWrites{
        .dstSet = m_lbmDescriptorSets[0],
        .dstBinding = 2,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .pBufferInfo = &voxelizationInfoBufferInfo};

    // Update descriptor sets with buffer information for Fluid Distribution Functions
    vk::DescriptorBufferInfo fluidDistributionBufferInfo{
        .buffer = m_fluidDistributionBuffer->getVkBuffer(),
        .offset = 0,
        .range = m_fluidDistributionBuffer->getSize()};
    vk::WriteDescriptorSet fluidDistributionDescriptorWrites{
        .dstSet = m_lbmDescriptorSets[0],
        .dstBinding = 3,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .pBufferInfo = &fluidDistributionBufferInfo};

    m_context.getDevice().logical().updateDescriptorSets(
        {lbmConfigDescriptorWrites, fluidInfoDescriptorWrites, voxelizationInfoDescriptorWrites,
         fluidDistributionDescriptorWrites},
        {});

    spdlog::info("Descriptor sets created and updated for LBM simulation.");
  }

  // Allocate descriptor sets for Marching Cubes
  {
    vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = *m_descriptorPool,
                                            .descriptorSetCount = 1,
                                            .pSetLayouts = &*m_marchCubesDescriptorSetLayout};
    m_marchCubesDescriptorSets = m_context.getDevice().logical().allocateDescriptorSets(allocInfo);

    // Update descriptor sets with buffer information for Fluid Info (e.g., tau, rho)
    vk::DescriptorBufferInfo fluidInfoBufferInfo{.buffer = m_fluidInfoBuffer->getVkBuffer(),
                                                 .offset = 0,
                                                 .range = m_fluidInfoBuffer->getSize()};
    vk::WriteDescriptorSet fluidInfoDescriptorWrites{.dstSet = m_marchCubesDescriptorSets[0],
                                                     .dstBinding = 0,
                                                     .dstArrayElement = 0,
                                                     .descriptorCount = 1,
                                                     .descriptorType =
                                                         vk::DescriptorType::eUniformBuffer,
                                                     .pBufferInfo = &fluidInfoBufferInfo};

    // Update descriptor sets with buffer information for Voxelization Info (e.g., aabb, cell size)
    vk::DescriptorBufferInfo voxelizationInfoBufferInfo{
        .buffer = m_voxelgrid->getVoxelizationInfoBuffer().getVkBuffer(),
        .offset = 0,
        .range = m_voxelgrid->getVoxelizationInfoBuffer().getSize()};
    vk::WriteDescriptorSet voxelizationInfoDescriptorWrites{
        .dstSet = m_marchCubesDescriptorSets[0],
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .pBufferInfo = &voxelizationInfoBufferInfo};

    // Update descriptor sets with buffer information for Voxel Grid
    vk::DescriptorBufferInfo voxelGridBufferInfo{
        .buffer = m_voxelgrid->getVoxelGridBuffer().getVkBuffer(),
        .offset = 0,
        .range = m_voxelgrid->getVoxelGridBuffer().getSize()};
    vk::WriteDescriptorSet voxelGridDescriptorWrites{.dstSet = m_marchCubesDescriptorSets[0],
                                                     .dstBinding = 2,
                                                     .dstArrayElement = 0,
                                                     .descriptorCount = 1,
                                                     .descriptorType =
                                                         vk::DescriptorType::eStorageBuffer,
                                                     .pBufferInfo = &voxelGridBufferInfo};

    // Update descriptor sets with buffer information for Position Buffer
    vk::DescriptorBufferInfo positionBufferInfo{.buffer = m_positionBuffer->getVkBuffer(),
                                                .offset = 0,
                                                .range = m_positionBuffer->getSize()};
    vk::WriteDescriptorSet positionDescriptorWrites{.dstSet = m_marchCubesDescriptorSets[0],
                                                    .dstBinding = 3,
                                                    .dstArrayElement = 0,
                                                    .descriptorCount = 1,
                                                    .descriptorType =
                                                        vk::DescriptorType::eStorageBuffer,
                                                    .pBufferInfo = &positionBufferInfo};

    // Update descriptor sets with buffer information for Normal Buffer
    vk::DescriptorBufferInfo normalBufferInfo{
        .buffer = m_normalBuffer->getVkBuffer(), .offset = 0, .range = m_normalBuffer->getSize()};
    vk::WriteDescriptorSet normalDescriptorWrites{.dstSet = m_marchCubesDescriptorSets[0],
                                                  .dstBinding = 4,
                                                  .dstArrayElement = 0,
                                                  .descriptorCount = 1,
                                                  .descriptorType =
                                                      vk::DescriptorType::eStorageBuffer,
                                                  .pBufferInfo = &normalBufferInfo};

    // Update descriptor sets with buffer information for Vertex Count Buffer
    vk::DescriptorBufferInfo vertexCountBufferInfo{.buffer = m_vertexCountBuffer->getVkBuffer(),
                                                   .offset = 0,
                                                   .range = m_vertexCountBuffer->getSize()};
    vk::WriteDescriptorSet vertexCountDescriptorWrites{.dstSet = m_marchCubesDescriptorSets[0],
                                                       .dstBinding = 5,
                                                       .dstArrayElement = 0,
                                                       .descriptorCount = 1,
                                                       .descriptorType =
                                                           vk::DescriptorType::eStorageBuffer,
                                                       .pBufferInfo = &vertexCountBufferInfo};

    m_context.getDevice().logical().updateDescriptorSets(
        {fluidInfoDescriptorWrites, voxelizationInfoDescriptorWrites, voxelGridDescriptorWrites,
         positionDescriptorWrites, normalDescriptorWrites, vertexCountDescriptorWrites},
        {});

    spdlog::info("Descriptor sets created and updated for Marching Cubes.");
  }
}

template <typename T, typename U>
  requires(std::is_same_v<T, D3Q15> || std::is_same_v<T, D3Q19>) && std::is_floating_point_v<U>
auto LBMFluid<T, U>::createPipeline() -> void
{
  // Create a Slang compiler instance for compiling Slang shaders to SPIR-V
  vksim::compiler::SlangCompiler slangCompiler(PROJECT_SOURCE_DIR "/src/shaders/physics");

  // Create compute pipeline for LBM fluid simulation
  {
    auto shaderCode = slangCompiler.compileToSpirv("lbmSolver.slang", "lbmSolver", "main");
    if (!shaderCode)
    {
      spdlog::error("{}", shaderCode.error().toString());
      std::abort();
    }

    vk::ShaderModuleCreateInfo createInfo{
        .codeSize = shaderCode->size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t *>(shaderCode->data())};
    vk::raii::ShaderModule shaderModule{m_context.getDevice().logical(), createInfo};

    m_lbmPipelineLayout =
        vk::raii::PipelineLayout(m_context.getDevice().logical(),
                                 {.setLayoutCount = 1, .pSetLayouts = &*m_lbmDescriptorSetLayout});

    vk::ComputePipelineCreateInfo pipelineCreateInfo{
        .stage = {.stage = vk::ShaderStageFlagBits::eCompute,
                  .module = *shaderModule,
                  .pName = "main"},
        .layout = *m_lbmPipelineLayout};

    m_lbmPipeline =
        vk::raii::Pipeline(m_context.getDevice().logical(), nullptr, pipelineCreateInfo);

    spdlog::info("Compute pipeline created for LBM fluid simulation.");
  }

  // Create compute pipeline for Marching Cubes
  {
    m_marchCubesPipelineLayout = vk::raii::PipelineLayout(
        m_context.getDevice().logical(),
        {.setLayoutCount = 1, .pSetLayouts = &*m_marchCubesDescriptorSetLayout});

    auto shaderCode = slangCompiler.compileToSpirv("marchCubes.slang", "marching_cubes", "main");
    if (!shaderCode)
    {
      spdlog::error("{}", shaderCode.error().toString());
      std::abort();
    }

    vk::ShaderModuleCreateInfo marchCubesCreateInfo{
        .codeSize = shaderCode->size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t *>(shaderCode->data())};
    vk::raii::ShaderModule marchCubesShaderModule{m_context.getDevice().logical(),
                                                  marchCubesCreateInfo};

    vk::ComputePipelineCreateInfo marchCubesPipelineCreateInfo{
        .stage = {.stage = vk::ShaderStageFlagBits::eCompute,
                  .module = *marchCubesShaderModule,
                  .pName = "main"},
        .layout = *m_marchCubesPipelineLayout};

    m_marchCubesPipeline =
        vk::raii::Pipeline(m_context.getDevice().logical(), nullptr, marchCubesPipelineCreateInfo);

    spdlog::info("Compute pipeline created for Marching Cubes.");
  }
}

template <typename T, typename U>
  requires(std::is_same_v<T, D3Q15> || std::is_same_v<T, D3Q19>) && std::is_floating_point_v<U>
auto LBMFluid<T, U>::getpositionBuffer() -> Buffer &
{
  return *m_positionBuffer;
}

template <typename T, typename U>
  requires(std::is_same_v<T, D3Q15> || std::is_same_v<T, D3Q19>) && std::is_floating_point_v<U>
auto LBMFluid<T, U>::getnormalBuffer() -> Buffer &
{
  return *m_normalBuffer;
}

template <typename T, typename U>
  requires(std::is_same_v<T, D3Q15> || std::is_same_v<T, D3Q19>) && std::is_floating_point_v<U>
auto LBMFluid<T, U>::recordCommandBuffer(vk::raii::CommandBuffer &commandBuffer) -> void
{
  // Record commands for LBM fluid simulation and Marching Cubes into the provided command buffer.

  // Barrier to ensure that the voxel grid buffer is ready for reading by the LBM solver
  vk::BufferMemoryBarrier2 before{.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                                  .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                  .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                                  .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                                  .buffer = *m_voxelgrid->getVoxelGridBuffer().getVkBuffer(),
                                  .offset = 0,
                                  .size = VK_WHOLE_SIZE};

  vk::DependencyInfo depBefore{.bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &before};
  commandBuffer.pipelineBarrier2(depBefore);

  // Bind the LBM compute pipeline and descriptor sets
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *m_lbmPipeline);
  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *m_lbmPipelineLayout, 0,
                                   *m_lbmDescriptorSets[0], {});

  // Dispatch compute shader for LBM fluid simulation. This assumes that the workgroup size is
  // 8x8x8, and the number of workgroups is calculated based on the voxel grid size.
  auto numWorkgroupsX =
      static_cast<uint32_t>(std::ceil(static_cast<float>(m_voxelgrid->getGridSize().x) / 8.0f));
  auto numWorkgroupsY =
      static_cast<uint32_t>(std::ceil(static_cast<float>(m_voxelgrid->getGridSize().y) / 8.0f));
  auto numWorkgroupsZ =
      static_cast<uint32_t>(std::ceil(static_cast<float>(m_voxelgrid->getGridSize().z) / 8.0f));
  commandBuffer.dispatch(numWorkgroupsX, numWorkgroupsY, numWorkgroupsZ);

  // Barrier to ensure that the voxel grid buffer is ready for reading by the Marching Cubes
  vk::BufferMemoryBarrier2 after{.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                                 .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                 .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                                 .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                                 .buffer = *m_voxelgrid->getVoxelGridBuffer().getVkBuffer(),
                                 .offset = 0,
                                 .size = VK_WHOLE_SIZE};

  vk::DependencyInfo depAfter{.bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &after};
  commandBuffer.pipelineBarrier2(depAfter);

  // Reset the vertex count buffer to zero before running Marching Cubes.
  commandBuffer.fillBuffer(m_vertexCountBuffer->getVkBuffer(), 0, sizeof(uint32_t), 0);

  // Barrier to ensure that the vertex count buffer is ready for reading by the Marching Cubes
  vk::BufferMemoryBarrier2 vertexCountBarrier{.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
                                              .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                                              .dstStageMask =
                                                  vk::PipelineStageFlagBits2::eComputeShader,
                                              .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                                              .buffer = *m_vertexCountBuffer->getVkBuffer(),
                                              .offset = 0,
                                              .size = sizeof(uint32_t)};
  vk::DependencyInfo vertexCountDep{.bufferMemoryBarrierCount = 1,
                                    .pBufferMemoryBarriers = &vertexCountBarrier};

  // Barrier to ensure that the position and normal buffers are ready for writing by the Marching
  // Cubes compute shader.
  vk::BufferMemoryBarrier2 positionBarrier{
      .srcStageMask = vk::PipelineStageFlagBits2::eVertexInput,
      .srcAccessMask = vk::AccessFlagBits2::eVertexAttributeRead,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
      .buffer = *m_positionBuffer->getVkBuffer(),
      .offset = 0,
      .size = m_positionBuffer->getSize()};
  vk::BufferMemoryBarrier2 normalBarrier{.srcStageMask = vk::PipelineStageFlagBits2::eVertexInput,
                                         .srcAccessMask = vk::AccessFlagBits2::eVertexAttributeRead,
                                         .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                                         .dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                         .buffer = *m_normalBuffer->getVkBuffer(),
                                         .offset = 0,
                                         .size = m_normalBuffer->getSize()};
  auto barriers = std::array<vk::BufferMemoryBarrier2, 2>{positionBarrier, normalBarrier};
  vk::DependencyInfo positionNormalDep{.bufferMemoryBarrierCount = 2,
                                       .pBufferMemoryBarriers = barriers.data()};

  // Record the barriers into the command buffer
  commandBuffer.pipelineBarrier2(vertexCountDep);
  commandBuffer.pipelineBarrier2(positionNormalDep);

  // Bind the Marching Cubes compute pipeline and descriptor sets
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *m_marchCubesPipeline);
  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *m_marchCubesPipelineLayout, 0,
                                   *m_marchCubesDescriptorSets[0], {});

  // Dispatch compute shader for Marching Cubes. This assumes that the workgroup size is 8x8x8,
  // and the number of workgroups is calculated based on the voxel grid size.
  auto numWorkgroupsXMC =
      static_cast<uint32_t>(std::ceil(static_cast<float>(m_voxelgrid->getGridSize().x) / 8.0f));
  auto numWorkgroupsYMC =
      static_cast<uint32_t>(std::ceil(static_cast<float>(m_voxelgrid->getGridSize().y) / 8.0f));
  auto numWorkgroupsZMC =
      static_cast<uint32_t>(std::ceil(static_cast<float>(m_voxelgrid->getGridSize().z) / 8.0f));
  commandBuffer.dispatch(numWorkgroupsXMC, numWorkgroupsYMC, numWorkgroupsZMC);
}

} // namespace vksim::physics
