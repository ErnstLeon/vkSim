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
  return pos == other.pos && normal == other.normal && uv == other.uv;
}

Mesh::Mesh(VulkanContext &context, const std::string &identifier, std::string filePath)
    : Resource(identifier), m_filePath(std::move(filePath)), m_context(context),
      m_positionsBuffer(context), m_normalsBuffer(context), m_uvsBuffer(context),
      m_indexBuffer(context)
{
}

auto Mesh::doLoad() -> bool
{
  loadFromFile();

  spdlog::info("Mesh {} loaded successfully with {} vertices and {} indices", GetId(),
               positions.size(), indices.size());

  return true;
}

auto Mesh::getPositionsBuffer() const -> const Buffer & { return m_positionsBuffer; }

auto Mesh::getNormalsBuffer() const -> const Buffer & { return m_normalsBuffer; }

auto Mesh::getUVsBuffer() const -> const Buffer & { return m_uvsBuffer; }

auto Mesh::getIndexBuffer() const -> const Buffer & { return m_indexBuffer; }

auto Mesh::getVertexBindingDescription() -> std::array<vk::VertexInputBindingDescription, 3>
{
  return {vk::VertexInputBindingDescription{
              .binding = 0, .stride = sizeof(glm::vec4), .inputRate = vk::VertexInputRate::eVertex},
          vk::VertexInputBindingDescription{
              .binding = 1, .stride = sizeof(glm::vec4), .inputRate = vk::VertexInputRate::eVertex},
          vk::VertexInputBindingDescription{.binding = 2,
                                            .stride = sizeof(glm::vec2),
                                            .inputRate = vk::VertexInputRate::eVertex}};
}

auto Mesh::getVertexAttributeDescriptions() -> std::array<vk::VertexInputAttributeDescription, 3>
{
  return {{{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = 0},
           {.location = 1, .binding = 1, .format = vk::Format::eR32G32B32Sfloat, .offset = 0},
           {.location = 2, .binding = 2, .format = vk::Format::eR32G32Sfloat, .offset = 0}}};
}

auto Mesh::getVertexCount() const -> size_t { return positions.size(); }

auto Mesh::getIndexCount() const -> size_t { return indices.size(); }

auto Mesh::getAABB() const -> std::pair<glm::vec3, glm::vec3>
{
  if (positions.empty())
  {
    spdlog::warn("Mesh {} has no vertices, returning default AABB", GetId());
    return {glm::vec3(0.0F), glm::vec3(0.0F)};
  }

  auto min = glm::vec3(positions[0]);
  auto max = glm::vec3(positions[0]);

  for (const auto &pos : positions)
  {
    min = glm::min(min, glm::vec3(pos));
    max = glm::max(max, glm::vec3(pos));
  }

  return {min, max};
}

auto Mesh::loadFromFile() -> void
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

      if (index.texcoord_index >= 0)
      {
        vertex.uv = {attrib.texcoords[(2 * index.texcoord_index) + 0],
                     1.0F - attrib.texcoords[(2 * index.texcoord_index) + 1]};
      }

      if (index.normal_index >= 0)
      {
        vertex.normal = {attrib.normals[(3 * index.normal_index) + 0],
                         attrib.normals[(3 * index.normal_index) + 1],
                         attrib.normals[(3 * index.normal_index) + 2]};
      }

      auto [iter, inserted] =
          uniqueVertices.try_emplace(vertex, static_cast<uint32_t>(positions.size()));

      if (inserted)
      {
        positions.emplace_back(vertex.pos, 1.0F);
        normals.emplace_back(vertex.normal, 0.0F);
        uvs.emplace_back(vertex.uv);
      }

      indices.push_back(iter->second);
    }

    // If there are no normals in the OBJ file, compute them manually
    if (attrib.normals.empty())
    {
      for (size_t i = 0; i < indices.size(); i += 3)
      {
        const auto &vert0 = positions[indices[i + 0]];
        const auto &vert1 = positions[indices[i + 1]];
        const auto &vert2 = positions[indices[i + 2]];

        glm::vec3 edge1 = glm::vec3(vert1) - glm::vec3(vert0);
        glm::vec3 edge2 = glm::vec3(vert2) - glm::vec3(vert0);
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

        normals[indices[i + 0]] += glm::vec4(normal, 0.0F);
        normals[indices[i + 1]] += glm::vec4(normal, 0.0F);
        normals[indices[i + 2]] += glm::vec4(normal, 0.0F);
      }

      // Normalize the normals
      for (auto &normal : normals)
      {
        normal = glm::normalize(normal);
      }
    }
  }

  // Create staging buffers for vertex and index data
  vk::DeviceSize positionsBufferSize = sizeof(positions[0]) * positions.size();
  vk::DeviceSize normalsBufferSize = sizeof(normals[0]) * normals.size();
  vk::DeviceSize uvsBufferSize = sizeof(uvs[0]) * uvs.size();
  vk::DeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();

  // Copy the vertex and index data to gpu buffers using staging buffers
  // Create the vertex buffer for positions
  m_positionsBuffer.create(BufferCreateInfo{.size = positionsBufferSize,
                                            .usage = vk::BufferUsageFlagBits::eVertexBuffer |
                                                     vk::BufferUsageFlagBits::eStorageBuffer |
                                                     vk::BufferUsageFlagBits::eTransferDst,
                                            .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                                            .debugName = "VertexPositionsBuffer"});

  m_positionsBuffer.copyFromHost(positions.data(), positionsBufferSize);

  // Create the vertex buffer for normals
  m_normalsBuffer.create(BufferCreateInfo{.size = normalsBufferSize,
                                          .usage = vk::BufferUsageFlagBits::eVertexBuffer |
                                                   vk::BufferUsageFlagBits::eTransferDst,
                                          .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                                          .debugName = "VertexNormalsBuffer"});

  m_normalsBuffer.copyFromHost(normals.data(), normalsBufferSize);

  // Create the vertex buffer for uvs
  m_uvsBuffer.create(BufferCreateInfo{.size = uvsBufferSize,
                                      .usage = vk::BufferUsageFlagBits::eVertexBuffer |
                                               vk::BufferUsageFlagBits::eTransferDst,
                                      .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                                      .debugName = "VertexUVsBuffer"});

  m_uvsBuffer.copyFromHost(uvs.data(), uvsBufferSize);

  // Create the index buffer
  m_indexBuffer.create(BufferCreateInfo{.size = indexBufferSize,
                                        .usage = vk::BufferUsageFlagBits::eIndexBuffer |
                                                 vk::BufferUsageFlagBits::eStorageBuffer |
                                                 vk::BufferUsageFlagBits::eTransferDst,
                                        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                                        .debugName = "IndexBuffer"});

  m_indexBuffer.copyFromHost(indices.data(), indexBufferSize);
}

} // namespace vksim