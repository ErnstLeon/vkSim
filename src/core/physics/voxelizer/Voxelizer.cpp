#include <optional>

#include "glm/ext/quaternion_common.hpp"
#include "glm/ext/vector_uint3_sized.hpp"
#include "glm/fwd.hpp"
#include "glm/matrix.hpp"
#include "vksim/core/physics/voxelizer/VoxelGrid.hpp"
#include "vksim/core/physics/voxelizer/Voxelizer.hpp"
#include "vksim/core/scene/Light.hpp"
#include "vksim/core/scene/Scene.hpp"
#include "vksim/slang/SlangCompiler.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim::physics
{
Voxelizer::Voxelizer(VulkanContext &context) : m_context(context) {}

auto Voxelizer::init(VoxelGrid &voxelGrid,
                     std::vector<std::reference_wrapper<SceneObject>> &sceneObjects) -> void
{
  // Store references to the scene objects and voxel grid for use in command buffer recording
  m_sceneObjects = sceneObjects;
  m_voxelGrid.emplace(voxelGrid);

  // Create the descriptor pool, descriptor set layout, descriptor sets, and compute pipeline for
  // voxelization.
  createDescriptorPool();
  createDescriptorSetLayout();
  createDescriptorSets();
  createPipeline();

  spdlog::info("Voxelizer initialized");
}

auto Voxelizer::recordCommandBuffer(vk::raii::CommandBuffer &commandBuffer) -> void
{
  // Ensure that the voxel grid buffer is cleared before starting the voxelization process
  vk::BufferMemoryBarrier2 before{.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader |
                                                  vk::PipelineStageFlagBits2::eFragmentShader,
                                  .srcAccessMask = vk::AccessFlagBits2::eShaderRead,
                                  .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
                                  .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
                                  .buffer = *m_voxelGrid->get().getVoxelGridBuffer().getVkBuffer(),
                                  .offset = 0,
                                  .size = VK_WHOLE_SIZE};

  vk::DependencyInfo depBefore{.bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &before};

  commandBuffer.pipelineBarrier2(depBefore);

  // Clear the voxel grid buffer to zero before starting the voxelization process
  commandBuffer.fillBuffer(*m_voxelGrid->get().getVoxelGridBuffer().getVkBuffer(), 0, VK_WHOLE_SIZE,
                           0);

  // Ensure that the voxel grid buffer is ready for use by the compute shader after clearing
  vk::BufferMemoryBarrier2 after{.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
                                 .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                                 .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                                 .dstAccessMask = vk::AccessFlagBits2::eShaderRead |
                                                  vk::AccessFlagBits2::eShaderWrite,
                                 .buffer = *m_voxelGrid->get().getVoxelGridBuffer().getVkBuffer(),
                                 .offset = 0,
                                 .size = VK_WHOLE_SIZE};

  vk::DependencyInfo depAfter{.bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &after};

  commandBuffer.pipelineBarrier2(depAfter);

  // Bind the compute pipeline and descriptor sets
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *m_pipeline);
  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *m_pipelineLayout, 0,
                                   *m_descriptorSets[0], {});

  // Dispatch the compute shader for each object with a mesh in the scene.
  struct ObjectInfo
  {
    glm::mat4 modelMatrix{glm::mat4(1.0F)}; // Model matrix of the object
    uint32_t objectId{0};                   // Unique identifier for the object
    uint32_t numFaces{0};                   // Number of faces in the object's mesh
    uint32_t numSamples{1};
  };
  ObjectInfo objectInfos;

  uint32_t objectId = 0;
  for (const auto &object : m_sceneObjects)
  {
    auto mesh = object.get().getResource<Mesh>();
    if (mesh)
    {
      auto facesCount = static_cast<uint32_t>(mesh.value()->getIndexCount() / 3);
      objectInfos = {.modelMatrix = glm::transpose(
                         glm::inverse(glm::transpose(object.get().getModelMatrix()))),
                     .objectId = objectId++,
                     .numFaces = facesCount,
                     .numSamples = 1};

      commandBuffer.pushConstants(m_pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0,
                                  sizeof(ObjectInfo), &objectInfos);

      // Parallelize over vertices and faces of the mesh using workgroups. Each subgroup will handle
      // one voxel, and each lane in the subgroup will handle a subset of the faces of the mesh.
      // Assuming a workgroup size of 8x8x8 threads
      auto numWorkgroupsX = static_cast<uint32_t>((m_voxelGrid->get().getGridSize().x + 7) / 8);
      auto numWorkgroupsY = static_cast<uint32_t>((m_voxelGrid->get().getGridSize().y + 7) / 8);
      auto numWorkgroupsZ = static_cast<uint32_t>((m_voxelGrid->get().getGridSize().z + 7) / 8);

      commandBuffer.dispatch(numWorkgroupsX, numWorkgroupsY, numWorkgroupsZ);
    }
  }
}

auto Voxelizer::createPipeline() -> void
{
  // Compile the Slang shader code to SPIR-V for the compute shader stage
  vksim::compiler::SlangCompiler slangCompiler(PROJECT_SOURCE_DIR "/src/shaders/physics");

  auto shaderCodeVert = slangCompiler.compileToSpirv("voxel.slang", "voxel", "main");
  if (!shaderCodeVert)
  {
    spdlog::error("{}", shaderCodeVert.error().toString());
    std::abort();
  };

  // Create a Vulkan shader module from the compiled SPIR-V code
  vk::ShaderModuleCreateInfo createInfo{
      .codeSize = shaderCodeVert->size() * sizeof(char),
      .pCode = reinterpret_cast<const uint32_t *>(shaderCodeVert->data())};
  vk::raii::ShaderModule shaderModule{m_context.getDevice().logical(), createInfo};

  // Create push constant to pass object ID and size of object's faces and model matrix to the
  // compute shader TO DO: Better use one single Buffer with all vertex data and global index buffer
  // instead of per object draw and voxelization
  vk::PushConstantRange pushConstantRange{.stageFlags = vk::ShaderStageFlagBits::eCompute,
                                          .offset = 0,
                                          .size = (sizeof(uint32_t) * 3) + sizeof(glm::mat4)};

  // Create the pipeline layout
  m_pipelineLayout = vk::raii::PipelineLayout(m_context.getDevice().logical(),
                                              {.setLayoutCount = 1,
                                               .pSetLayouts = &*m_descriptorSetLayout,
                                               .pushConstantRangeCount = 1,
                                               .pPushConstantRanges = &pushConstantRange});

  // Create the compute pipeline
  vk::ComputePipelineCreateInfo pipelineCreateInfo{
      .stage = {.stage = vk::ShaderStageFlagBits::eCompute,
                .module = *shaderModule,
                .pName = "main"},
      .layout = *m_pipelineLayout};

  m_pipeline = vk::raii::Pipeline(m_context.getDevice().logical(), nullptr, pipelineCreateInfo);

  spdlog::info("Compute pipeline created for voxelization.");
}

auto Voxelizer::createDescriptorPool() -> void
{
  // Iterate though the scene objects and count the number of object with meshes to determine the
  // number of storage buffers needed for the descriptor pool. Create a descriptor pool with 1
  // uniform info buffer, 1 storage buffer for the voxel grid plus 2 storage buffer for each
  // object with a mesh (positions
  // + indices).
  std::array<vk::DescriptorPoolSize, 2> poolSize;
  poolSize[0] =
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1};
  poolSize[1] =
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 1};
  for (const auto &object : m_sceneObjects)
  {
    if (object.get().getResource<Mesh>())
    {
      // Each object with a mesh will require a storage buffer for its voxel grid.
      // Increment the descriptor count for each such object.
      poolSize[1].descriptorCount += 2; // 1 for positions + 1 for indices
    }
  }

  vk::DescriptorPoolCreateInfo poolCreateInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = 1,
      .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
      .pPoolSizes = poolSize.data()};

  m_descriptorPool = vk::raii::DescriptorPool(m_context.getDevice().logical(), poolCreateInfo);

  spdlog::info(
      "Descriptor pool created for voxelization with {} uniform buffer(s) and {} storage buffer(s)",
      poolSize[0].descriptorCount, poolSize[1].descriptorCount);
}

auto Voxelizer::createDescriptorSetLayout() -> void
{
  // Get the number of objects with meshes in the scene to determine the descriptor count for
  // binding 1.
  uint32_t objectsCount = 0;
  for (const auto &object : m_sceneObjects)
  {
    if (object.get().getResource<Mesh>())
    {
      objectsCount++;
    }
  }

  // Create descriptor set layout bindings for the voxel grid and object storage buffers.
  // Binding 0: Uniform buffer for voxelization parameters (cell size and AABB).
  // Binding 1: Storage buffer for the voxel grid.
  // Binding 2: Storage buffer array for each object with a mesh in the scene (positions).
  // Binding 3: Storage buffer array for each object with a mesh in the scene (indices).
  std::array<vk::DescriptorSetLayoutBinding, 4> bindings = {{
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
       .descriptorCount = objectsCount,
       .stageFlags = vk::ShaderStageFlagBits::eCompute},
      {.binding = 3,
       .descriptorType = vk::DescriptorType::eStorageBuffer,
       .descriptorCount = objectsCount,
       .stageFlags = vk::ShaderStageFlagBits::eCompute},
  }};

  vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{
      .bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data()};

  m_descriptorSetLayout =
      vk::raii::DescriptorSetLayout(m_context.getDevice().logical(), layoutCreateInfo);

  spdlog::info("Descriptor set layout created with {} bindings", bindings.size());
}

auto Voxelizer::createDescriptorSets() -> void
{
  // Allocate descriptor sets from the descriptor pool using the descriptor set layout.
  vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = *m_descriptorPool,
                                          .descriptorSetCount = 1,
                                          .pSetLayouts = &*m_descriptorSetLayout};

  m_descriptorSets = vk::raii::DescriptorSets(m_context.getDevice().logical(), allocInfo);

  // Update the descriptor set for the voxelization parameters uniform buffer.
  vk::DescriptorBufferInfo voxelizationParamsBufferInfo{
      .buffer = m_voxelGrid->get().getVoxelizationParamsBuffer().getVkBuffer(),
      .offset = 0,
      .range = sizeof(glm::vec4) * 2};

  vk::WriteDescriptorSet paramsDescriptorWrites = {.dstSet = m_descriptorSets[0],
                                                   .dstBinding = 0,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType =
                                                       vk::DescriptorType::eUniformBuffer,
                                                   .pBufferInfo = &voxelizationParamsBufferInfo};

  // Update the descriptor sets with the voxel grid buffer.
  vk::DescriptorBufferInfo voxelGridBufferInfo{
      .buffer = m_voxelGrid->get().getVoxelGridBuffer().getVkBuffer(),
      .offset = 0,
      .range = m_voxelGrid->get().getVoxelGridBuffer().getSize()};

  vk::WriteDescriptorSet gridDescriptorWrites = {.dstSet = m_descriptorSets[0],
                                                 .dstBinding = 1,
                                                 .dstArrayElement = 0,
                                                 .descriptorCount = 1,
                                                 .descriptorType =
                                                     vk::DescriptorType::eStorageBuffer,
                                                 .pBufferInfo = &voxelGridBufferInfo};

  // Update the descriptor sets for the uniform grid information and voxel grid storage buffer.
  m_context.getDevice().logical().updateDescriptorSets(
      {gridDescriptorWrites, paramsDescriptorWrites}, {});

  // Update the descriptor set for the object storage buffers.
  std::vector<vk::DescriptorBufferInfo> objectPositionsBufferInfos;
  std::vector<vk::DescriptorBufferInfo> objectIndicesBufferInfos;
  for (const auto &object : m_sceneObjects)
  {
    auto mesh = object.get().getResource<Mesh>();
    if (mesh)
    {
      // Assuming each object has a storage buffer for its voxel grid.
      // Replace this with the actual buffer associated with the object.
      const auto &objectPositionsBuffer = mesh.value()->getPositionsBuffer();
      vk::DescriptorBufferInfo objectPositionsBufferInfo{.buffer =
                                                             objectPositionsBuffer.getVkBuffer(),
                                                         .offset = 0,
                                                         .range = objectPositionsBuffer.getSize()};
      objectPositionsBufferInfos.push_back(objectPositionsBufferInfo);

      const auto &objectIndicesBuffer = mesh.value()->getIndexBuffer();
      vk::DescriptorBufferInfo objectIndicesBufferInfo{.buffer = objectIndicesBuffer.getVkBuffer(),
                                                       .offset = 0,
                                                       .range = objectIndicesBuffer.getSize()};
      objectIndicesBufferInfos.push_back(objectIndicesBufferInfo);
    }
  }

  std::array<vk::WriteDescriptorSet, 2> objectDescriptorWrites = {
      {{.dstSet = m_descriptorSets[0],
        .dstBinding = 2,
        .dstArrayElement = 0,
        .descriptorCount = static_cast<uint32_t>(objectPositionsBufferInfos.size()),
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .pBufferInfo = objectPositionsBufferInfos.data()},
       {.dstSet = m_descriptorSets[0],
        .dstBinding = 3,
        .dstArrayElement = 0,
        .descriptorCount = static_cast<uint32_t>(objectIndicesBufferInfos.size()),
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .pBufferInfo = objectIndicesBufferInfos.data()}},
  };

  m_context.getDevice().logical().updateDescriptorSets(objectDescriptorWrites, {});

  spdlog::info("Descriptor sets created and updated for voxelization parameters, voxel grid, and "
               "object storage buffers.");
};

} // namespace vksim::physics
