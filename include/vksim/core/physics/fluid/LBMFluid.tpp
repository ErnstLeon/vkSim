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

  // Create buffer for fluid properties (tau, rho, etc.).
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

  m_LBMConfigBuffer->copyFromHost(&m_latticeConfig, sizeof(m_latticeConfig));
}

template <typename T, typename U>
  requires(std::is_same_v<T, D3Q15> || std::is_same_v<T, D3Q19>) && std::is_floating_point_v<U>
auto LBMFluid<T, U>::createDescriptorPool() -> void
{
  // LBM + Marching Cubes resources in one pool.
  std::array<vk::DescriptorPoolSize, 2> poolSize;
  poolSize[0] =
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = 3};
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
  std::array<vk::DescriptorSetLayoutBinding, 3> bindings = {{
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
  }};

  vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{
      .bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data()};

  m_descriptorSetLayout =
      vk::raii::DescriptorSetLayout(m_context.getDevice().logical(), layoutCreateInfo);

  spdlog::info("Descriptor set layout created with {} bindings", bindings.size());

  std::array<vk::DescriptorSetLayoutBinding, 5> marchCubesBindings = {{
      {.binding = 0,
       .descriptorType = vk::DescriptorType::eUniformBuffer,
       .descriptorCount = 1,
       .stageFlags = vk::ShaderStageFlagBits::eCompute},
      {.binding = 1,
       .descriptorType = vk::DescriptorType::eStorageBuffer,
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
  }};

  vk::DescriptorSetLayoutCreateInfo marchCubesLayoutCreateInfo{
      .bindingCount = static_cast<uint32_t>(marchCubesBindings.size()),
      .pBindings = marchCubesBindings.data()};

  m_marchCubesDescriptorSetLayout =
      vk::raii::DescriptorSetLayout(m_context.getDevice().logical(), marchCubesLayoutCreateInfo);

  spdlog::info("Marching Cubes descriptor set layout created with {} bindings",
               marchCubesBindings.size());
}

template <typename T, typename U>
  requires(std::is_same_v<T, D3Q15> || std::is_same_v<T, D3Q19>) && std::is_floating_point_v<U>
auto LBMFluid<T, U>::createDescriptorSets() -> void
{
  vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = *m_descriptorPool,
                                          .descriptorSetCount = 1,
                                          .pSetLayouts = &*m_descriptorSetLayout};

  m_descriptorSets = m_context.getDevice().logical().allocateDescriptorSets(allocInfo);

  vk::DescriptorBufferInfo lbmConfigBufferInfo{
      .buffer = m_LBMConfigBuffer->getVkBuffer(), .offset = 0, .range = sizeof(m_latticeConfig)};

  vk::WriteDescriptorSet paramsDescriptorWrites{.dstSet = m_descriptorSets[0],
                                                .dstBinding = 0,
                                                .dstArrayElement = 0,
                                                .descriptorCount = 1,
                                                .descriptorType =
                                                    vk::DescriptorType::eUniformBuffer,
                                                .pBufferInfo = &lbmConfigBufferInfo};

  vk::DescriptorBufferInfo fluidInfoBufferInfo{
      .buffer = m_fluidInfoBuffer->getVkBuffer(), .offset = 0, .range = sizeof(FluidInfo)};

  vk::WriteDescriptorSet gridDescriptorWrites{.dstSet = m_descriptorSets[0],
                                              .dstBinding = 1,
                                              .dstArrayElement = 0,
                                              .descriptorCount = 1,
                                              .descriptorType = vk::DescriptorType::eUniformBuffer,
                                              .pBufferInfo = &fluidInfoBufferInfo};

  vk::DescriptorBufferInfo fluidDistributionBufferInfo{
      .buffer = m_fluidDistributionBuffer->getVkBuffer(),
      .offset = 0,
      .range = m_fluidDistributionBuffer->getSize()};

  vk::WriteDescriptorSet fluidDistributionDescriptorWrites{
      .dstSet = m_descriptorSets[0],
      .dstBinding = 2,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = vk::DescriptorType::eStorageBuffer,
      .pBufferInfo = &fluidDistributionBufferInfo};

  m_context.getDevice().logical().updateDescriptorSets(
      {gridDescriptorWrites, paramsDescriptorWrites, fluidDistributionDescriptorWrites}, {});

  spdlog::info("Descriptor sets created and updated for LBM simulation.");

  vk::DescriptorSetAllocateInfo mallocInfo{.descriptorPool = *m_descriptorPool,
                                           .descriptorSetCount = 1,
                                           .pSetLayouts = &*m_marchCubesDescriptorSetLayout};

  m_marchCubesDescriptorSet = m_context.getDevice().logical().allocateDescriptorSets(mallocInfo);

  vk::DescriptorBufferInfo voxelizationParamsBufferInfo{
      .buffer = m_voxelgrid->getVoxelizationParamsBuffer().getVkBuffer(),
      .offset = 0,
      .range = m_voxelgrid->getVoxelizationParamsBuffer().getSize()};

  vk::WriteDescriptorSet voxelizationParamsDescriptorWrites{
      .dstSet = m_marchCubesDescriptorSet[0],
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = vk::DescriptorType::eUniformBuffer,
      .pBufferInfo = &voxelizationParamsBufferInfo};

  m_context.getDevice().logical().updateDescriptorSets({voxelizationParamsDescriptorWrites}, {});

  vk::DescriptorBufferInfo voxelGridBufferInfo{
      .buffer = m_voxelgrid->getVoxelGridBuffer().getVkBuffer(),
      .offset = 0,
      .range = m_voxelgrid->getVoxelGridBuffer().getSize()};

  vk::WriteDescriptorSet voxelGridDescriptorWrites{.dstSet = m_marchCubesDescriptorSet[0],
                                                   .dstBinding = 1,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType =
                                                       vk::DescriptorType::eStorageBuffer,
                                                   .pBufferInfo = &voxelGridBufferInfo};
  m_context.getDevice().logical().updateDescriptorSets({voxelGridDescriptorWrites}, {});

  vk::DescriptorBufferInfo positionBufferInfo{
      .buffer = m_positionBuffer->getVkBuffer(), .offset = 0, .range = m_positionBuffer->getSize()};

  vk::WriteDescriptorSet positionDescriptorWrites{.dstSet = m_marchCubesDescriptorSet[0],
                                                  .dstBinding = 2,
                                                  .dstArrayElement = 0,
                                                  .descriptorCount = 1,
                                                  .descriptorType =
                                                      vk::DescriptorType::eStorageBuffer,
                                                  .pBufferInfo = &positionBufferInfo};
  m_context.getDevice().logical().updateDescriptorSets({positionDescriptorWrites}, {});

  vk::DescriptorBufferInfo normalBufferInfo{
      .buffer = m_normalBuffer->getVkBuffer(), .offset = 0, .range = m_normalBuffer->getSize()};

  vk::WriteDescriptorSet normalDescriptorWrites{.dstSet = m_marchCubesDescriptorSet[0],
                                                .dstBinding = 3,
                                                .dstArrayElement = 0,
                                                .descriptorCount = 1,
                                                .descriptorType =
                                                    vk::DescriptorType::eStorageBuffer,
                                                .pBufferInfo = &normalBufferInfo};
  m_context.getDevice().logical().updateDescriptorSets({normalDescriptorWrites}, {});

  vk::DescriptorBufferInfo vertexCountBufferInfo{.buffer = m_vertexCountBuffer->getVkBuffer(),
                                                 .offset = 0,
                                                 .range = m_vertexCountBuffer->getSize()};

  vk::WriteDescriptorSet vertexCountDescriptorWrites{.dstSet = m_marchCubesDescriptorSet[0],
                                                     .dstBinding = 4,
                                                     .dstArrayElement = 0,
                                                     .descriptorCount = 1,
                                                     .descriptorType =
                                                         vk::DescriptorType::eStorageBuffer,
                                                     .pBufferInfo = &vertexCountBufferInfo};
  m_context.getDevice().logical().updateDescriptorSets({vertexCountDescriptorWrites}, {});
}

template <typename T, typename U>
  requires(std::is_same_v<T, D3Q15> || std::is_same_v<T, D3Q19>) && std::is_floating_point_v<U>
auto LBMFluid<T, U>::createPipeline() -> void
{
  vksim::compiler::SlangCompiler slangCompiler(PROJECT_SOURCE_DIR "/src/shaders/physics");

  auto shaderCodeVert = slangCompiler.compileToSpirv("lbmSolver.slang", "lbm", "main");
  if (!shaderCodeVert)
  {
    spdlog::error("{}", shaderCodeVert.error().toString());
    std::abort();
  }

  vk::ShaderModuleCreateInfo createInfo{
      .codeSize = shaderCodeVert->size() * sizeof(char),
      .pCode = reinterpret_cast<const uint32_t *>(shaderCodeVert->data())};
  vk::raii::ShaderModule shaderModule{m_context.getDevice().logical(), createInfo};

  m_pipelineLayout =
      vk::raii::PipelineLayout(m_context.getDevice().logical(),
                               {.setLayoutCount = 1, .pSetLayouts = &*m_descriptorSetLayout});

  vk::ComputePipelineCreateInfo pipelineCreateInfo{
      .stage = {.stage = vk::ShaderStageFlagBits::eCompute,
                .module = *shaderModule,
                .pName = "main"},
      .layout = *m_pipelineLayout};

  m_pipeline = vk::raii::Pipeline(m_context.getDevice().logical(), nullptr, pipelineCreateInfo);

  spdlog::info("Compute pipeline created for LBM fluid simulation.");

  m_marchCubesPipelineLayout = vk::raii::PipelineLayout(
      m_context.getDevice().logical(),
      {.setLayoutCount = 1, .pSetLayouts = &*m_marchCubesDescriptorSetLayout});

  auto shaderCodeMarchCubes =
      slangCompiler.compileToSpirv("marchCubes.slang", "marching_cubes", "main");
  if (!shaderCodeMarchCubes)
  {
    spdlog::error("{}", shaderCodeMarchCubes.error().toString());
    std::abort();
  }

  vk::ShaderModuleCreateInfo marchCubesCreateInfo{
      .codeSize = shaderCodeMarchCubes->size() * sizeof(char),
      .pCode = reinterpret_cast<const uint32_t *>(shaderCodeMarchCubes->data())};
  vk::raii::ShaderModule marchCubesShaderModule{m_context.getDevice().logical(),
                                                marchCubesCreateInfo};

  vk::ComputePipelineCreateInfo marchCubesPipelineCreateInfo{
      .stage = {.stage = vk::ShaderStageFlagBits::eCompute,
                .module = *marchCubesShaderModule,
                .pName = "main"},
      .layout = *m_marchCubesPipelineLayout};

  m_marchCubesPipeline =
      vk::raii::Pipeline(m_context.getDevice().logical(), nullptr, marchCubesPipelineCreateInfo);
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
  vk::BufferMemoryBarrier2 before{.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                                  .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                  .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                                  .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                                  .buffer = *m_voxelgrid->getVoxelGridBuffer().getVkBuffer(),
                                  .offset = 0,
                                  .size = VK_WHOLE_SIZE};

  vk::DependencyInfo depBefore{.bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &before};
  commandBuffer.pipelineBarrier2(depBefore);

  commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *m_pipeline);
  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *m_pipelineLayout, 0,
                                   *m_descriptorSets[0], {});

  auto numWorkgroupsX =
      static_cast<uint32_t>(std::ceil(static_cast<float>(m_voxelgrid->getGridSize().x) / 8.0f));
  auto numWorkgroupsY =
      static_cast<uint32_t>(std::ceil(static_cast<float>(m_voxelgrid->getGridSize().y) / 8.0f));
  auto numWorkgroupsZ =
      static_cast<uint32_t>(std::ceil(static_cast<float>(m_voxelgrid->getGridSize().z) / 8.0f));
  commandBuffer.dispatch(numWorkgroupsX, numWorkgroupsY, numWorkgroupsZ);

  vk::BufferMemoryBarrier2 after{.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                                 .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                 .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                                 .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                                 .buffer = *m_voxelgrid->getVoxelGridBuffer().getVkBuffer(),
                                 .offset = 0,
                                 .size = VK_WHOLE_SIZE};

  vk::DependencyInfo depAfter{.bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &after};
  commandBuffer.pipelineBarrier2(depAfter);

  commandBuffer.fillBuffer(m_vertexCountBuffer->getVkBuffer(), 0, sizeof(uint32_t), 0);

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

  commandBuffer.pipelineBarrier2(vertexCountDep);
  commandBuffer.pipelineBarrier2(positionNormalDep);

  commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *m_marchCubesPipeline);
  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *m_marchCubesPipelineLayout, 0,
                                   *m_marchCubesDescriptorSet[0], {});

  auto numWorkgroupsXMC =
      static_cast<uint32_t>(std::ceil(static_cast<float>(m_voxelgrid->getGridSize().x) / 8.0f));
  auto numWorkgroupsYMC =
      static_cast<uint32_t>(std::ceil(static_cast<float>(m_voxelgrid->getGridSize().y) / 8.0f));
  auto numWorkgroupsZMC =
      static_cast<uint32_t>(std::ceil(static_cast<float>(m_voxelgrid->getGridSize().z) / 8.0f));
  commandBuffer.dispatch(numWorkgroupsXMC, numWorkgroupsYMC, numWorkgroupsZMC);
}

} // namespace vksim::physics
