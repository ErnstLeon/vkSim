#include <expected>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/buffers/Buffer.hpp"
#include "vksim/core/buffers/Image.hpp"
#include "vksim/core/resources/Mesh.hpp"
#include "vksim/core/resources/Resource.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim
{

auto Vertex::operator==(const Vertex &other) const -> bool
{
  return pos == other.pos && color == other.color && uv == other.uv;
}

auto Vertex::getBindingDescription() -> vk::VertexInputBindingDescription
{
  return vk::VertexInputBindingDescription{
      .binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
}

auto Vertex::getAttributeDescriptions() -> std::array<vk::VertexInputAttributeDescription, 3>
{
  return {{{.location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = offsetof(Vertex, pos)},
           {.location = 1,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = offsetof(Vertex, color)},
           {.location = 2,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = offsetof(Vertex, uv)}}};
}

Mesh::Mesh(const std::string &identifier, std::string filePath, VulkanContext *context)
    : Resource(identifier), m_filePath(std::move(filePath)), m_context(context)
{
}

auto Mesh::doLoad(UploadContext &uploadContext) -> bool
{
  loadFromFile(uploadContext);

  spdlog::info("Mesh {} loaded successfully with {} vertices and {} indices", GetId(),
               vertices.size(), indices.size());

  return true;
}

auto Mesh::doUnload() -> bool
{
  m_vertexBuffer = Buffer();
  m_indexBuffer = Buffer();

  vertices.clear();
  indices.clear();
  return true;
}

auto Mesh::getVertexBuffer() const -> const vk::raii::Buffer &
{
  return m_vertexBuffer.getVkBuffer();
}

auto Mesh::getIndexBuffer() const -> const vk::raii::Buffer &
{
  return m_indexBuffer.getVkBuffer();
}

auto Mesh::getVertexBindingDescription() -> vk::VertexInputBindingDescription
{
  return Vertex::getBindingDescription();
}

auto Mesh::getVertexAttributeDescriptions() -> std::array<vk::VertexInputAttributeDescription, 3>
{
  return Vertex::getAttributeDescriptions();
}

auto Mesh::getVertexCount() const -> size_t { return vertices.size(); }

auto Mesh::getIndexCount() const -> size_t { return indices.size(); }

auto Mesh::loadFromFile(UploadContext &uploadContext) -> void
{
  // Load the mesh using tinyobjloader
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn;
  std::string err;

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, m_filePath.c_str()))
  {
    spdlog::error("Failed to load obj mesh from file!");
    std::abort();
  }

  // Use a hash map to ensure unique vertices and build the index buffer
  std::unordered_map<Vertex, uint32_t> uniqueVertices{};

  for (const auto &shape : shapes)
  {
    for (const auto &index : shape.mesh.indices)
    {
      Vertex vertex{};

      vertex.pos = {attrib.vertices[(3 * index.vertex_index) + 0],
                    attrib.vertices[(3 * index.vertex_index) + 1],
                    attrib.vertices[(3 * index.vertex_index) + 2]};

      vertex.uv = {attrib.texcoords[(2 * index.texcoord_index) + 0],
                   1.0F - attrib.texcoords[(2 * index.texcoord_index) + 1]};

      vertex.color = {1.0F, 1.0F, 1.0F};

      auto [iter, inserted] =
          uniqueVertices.try_emplace(vertex, static_cast<uint32_t>(vertices.size()));

      if (inserted)
      {
        vertices.push_back(vertex);
      }

      indices.push_back(iter->second);
    }
  }

  // Create staging buffers for vertex and index data
  vk::DeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
  auto stagingVertexBuffer =
      Buffer(m_context, BufferCreateInfo{.size = vertexBufferSize,
                                         .usage = vk::BufferUsageFlagBits::eTransferSrc,
                                         .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                                       vk::MemoryPropertyFlagBits::eHostCoherent});
  auto stagingIndexBuffer =
      Buffer(m_context, BufferCreateInfo{.size = sizeof(indices[0]) * indices.size(),
                                         .usage = vk::BufferUsageFlagBits::eTransferSrc,
                                         .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                                       vk::MemoryPropertyFlagBits::eHostCoherent});

  // Map the staging buffers and copy the vertex and index data into them
  auto *stagingVertexBufferMemory =
      stagingVertexBuffer.getVkBufferMemory().mapMemory(0, vertexBufferSize);
  std::memcpy(stagingVertexBufferMemory, vertices.data(), static_cast<size_t>(vertexBufferSize));
  stagingVertexBuffer.getVkBufferMemory().unmapMemory();

  auto *stagingIndexBufferMemory =
      stagingIndexBuffer.getVkBufferMemory().mapMemory(0, sizeof(indices[0]) * indices.size());
  std::memcpy(stagingIndexBufferMemory, indices.data(),
              static_cast<size_t>(sizeof(indices[0]) * indices.size()));
  stagingIndexBuffer.getVkBufferMemory().unmapMemory();

  // Create the vertex and index buffers on the GPU with device-local memory
  m_vertexBuffer =
      Buffer(m_context, BufferCreateInfo{.size = vertexBufferSize,
                                         .usage = vk::BufferUsageFlagBits::eVertexBuffer |
                                                  vk::BufferUsageFlagBits::eTransferDst,
                                         .properties = vk::MemoryPropertyFlagBits::eDeviceLocal});

  m_indexBuffer =
      Buffer(m_context, BufferCreateInfo{.size = sizeof(indices[0]) * indices.size(),
                                         .usage = vk::BufferUsageFlagBits::eIndexBuffer |
                                                  vk::BufferUsageFlagBits::eTransferDst,
                                         .properties = vk::MemoryPropertyFlagBits::eDeviceLocal});

  // Copy the data from the staging buffers to the GPU buffers using the provided command buffer
  m_vertexBuffer.copyFromBuffer(stagingVertexBuffer, vertexBufferSize,
                                uploadContext.getCommandBuffer());
  m_indexBuffer.copyFromBuffer(stagingIndexBuffer, sizeof(indices[0]) * indices.size(),
                               uploadContext.getCommandBuffer());

  // Add the staging buffers to the UploadContext to ensure they remain alive until the upload is
  // complete
  uploadContext.addStagingBuffer(std::move(stagingVertexBuffer));
  uploadContext.addStagingBuffer(std::move(stagingIndexBuffer));
}

} // namespace vksim