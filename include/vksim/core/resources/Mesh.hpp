#pragma once

#include <expected>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/buffers/Buffer.hpp"
#include "vksim/core/buffers/Image.hpp"
#include "vksim/core/resources/Resource.hpp"

namespace vksim
{
/** @brief Structure representing a vertex with position, normal, and texture coordinates.
 * @note This structure is used when reading mesh data from files and is not used for rendering or
 * computations, there we use SOA and float4 padding for better alignment in computations. */
struct Vertex
{
  glm::vec3 pos{};
  glm::vec3 normal{};
  glm::vec2 uv{};

  auto operator==(const Vertex &other) const -> bool;
};
} // namespace vksim

namespace std
{
/** @brief Hash function specialization for the Vertex structure.
 * This allows Vertex to be used as a key in unordered containers.
 */
template <> struct hash<vksim::Vertex>
{
  auto operator()(vksim::Vertex const &vertex) const -> size_t
  {
    return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
           (hash<glm::vec2>()(vertex.uv) << 1);
  }
};
} // namespace std

namespace vksim
{
/** @brief A class representing a mesh resource.
 * This class encapsulates the properties and behaviors of a 3D mesh, including its vertex and index
 * buffers, as well as methods for loading the mesh from a file and computing its axis-aligned
 * bounding box (AABB).
 * @note The vertex data is
 * stored in separate buffers for positions, normals, and texture coordinates (SOA layout). The
 * index buffer is also created to define the mesh's topology.
 */
class Mesh : public Resource
{
public:
  /** @brief Constructs a new mesh resource.
   * @param context Reference to the Vulkan context for resource management.
   * @param identifier Unique identifier for the mesh.
   * @param filePath Path to the mesh file.
   */
  explicit Mesh(VulkanContext &context, const std::string &identifier, std::string filePath);

  Mesh(const Mesh &) = delete;
  Mesh(Mesh &&) noexcept = default;

  auto operator=(const Mesh &) -> Mesh & = delete;
  auto operator=(Mesh &&) -> Mesh & = delete;

  ~Mesh() override = default;

  /** @brief Loads the mesh resource.
   * @return True if the mesh was successfully loaded, false otherwise.
   */
  auto doLoad() -> bool override;

  /** @brief Returns the vertex buffers for the mesh.
   * @return References to the vertex buffers.
   */
  [[nodiscard]] auto getPositionsBuffer() const -> const Buffer &;
  [[nodiscard]] auto getNormalsBuffer() const -> const Buffer &;
  [[nodiscard]] auto getUVsBuffer() const -> const Buffer &;

  /** @brief Returns the index buffer for the mesh.
   * @return Reference to the index buffer.
   */
  [[nodiscard]] auto getIndexBuffer() const -> const Buffer &;

  /** @brief Gets the binding description for the vertex input.
   * @return The vertex input binding description.
   */
  static auto getVertexBindingDescription() -> std::array<vk::VertexInputBindingDescription, 3>;

  /** @brief Gets the attribute descriptions for the vertex input.
   * @return An array of vertex input attribute descriptions.
   */
  static auto getVertexAttributeDescriptions()
      -> std::array<vk::VertexInputAttributeDescription, 3>;

  /** @brief Returns the number of vertices in the mesh.
   * @return The vertex count.
   */
  [[nodiscard]] auto getVertexCount() const -> size_t;

  /** @brief Returns the number of indices in the mesh.
   * @return The index count.
   */
  [[nodiscard]] auto getIndexCount() const -> size_t;

  /** @brief Computes the axis-aligned bounding box (AABB) of the mesh based on its vertices.
   * @return A pair of glm::vec3 representing the minimum and maximum corners of the AABB.
   */
  [[nodiscard]]
  auto getAABB() const -> std::pair<glm::vec3, glm::vec3>;

private:
  /** @brief Loads the mesh from a file and creates the necessary Vulkan resources.
   */
  auto loadFromFile() -> void;

  Buffer m_positionsBuffer; // Vertex position buffer for the mesh
  Buffer m_normalsBuffer;   // Vertex normal buffer for the mesh
  Buffer m_uvsBuffer;       // Vertex texture coordinate buffer for the mesh
  Buffer m_indexBuffer;     // Index buffer for the mesh

  std::vector<glm::vec4> positions; // Vertex positions of the mesh
  std::vector<glm::vec4> normals;   // Vertex normals of the mesh
  std::vector<glm::vec2> uvs;       // Vertex texture coordinates of the mesh

  std::vector<uint32_t> indices; // Indices of the mesh

  std::string m_filePath;   // Path to the mesh file
  VulkanContext &m_context; // Reference to the Vulkan context for resource management
};
} // namespace vksim