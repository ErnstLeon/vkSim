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
/** @brief Structure to hold information for creating a mesh resource.
 */
struct Vertex
{
  glm::vec3 pos{};
  glm::vec3 color{};
  glm::vec2 uv{};

  auto operator==(const Vertex &other) const -> bool;

  /** @brief Returns the binding description for the vertex input.
   * @return The vertex input binding description.
   */
  static auto getBindingDescription() -> vk::VertexInputBindingDescription;

  /** @brief Returns the attribute descriptions for the vertex input.
   * @return An array of vertex input attribute descriptions.
   */
  static auto getAttributeDescriptions() -> std::array<vk::VertexInputAttributeDescription, 3>;
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
    return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
           (hash<glm::vec2>()(vertex.uv) << 1);
  }
};
} // namespace std

namespace vksim
{
/** @brief A class representing a mesh resource. */
class Mesh : public Resource
{
public:
  /** @brief Constructs a new mesh resource.
   * @param identifier Unique identifier for the mesh.
   * @param filePath Path to the mesh file.
   * @param context Pointer to the Vulkan context for access to GPU resources.
   */
  explicit Mesh(const std::string &identifier, std::string filePath, VulkanContext *context);

  Mesh() = default;

  Mesh(const Mesh &) = delete;
  Mesh(Mesh &&) noexcept = default;

  auto operator=(const Mesh &) -> Mesh & = delete;
  auto operator=(Mesh &&) -> Mesh & = default;

  ~Mesh() override = default;

  /** @brief Loads the mesh resource.
   * @param uploadContext Upload command context that keeps staging buffers alive.
   * @return True if the mesh was successfully loaded, false otherwise.
   */
  auto doLoad(UploadContext &uploadContext) -> bool override;

  /** @brief Unloads the mesh resource, releasing GPU resources.
   * @return True if the mesh was successfully unloaded, false otherwise.
   */
  auto doUnload() -> bool override;

  /** @brief Returns the vertex buffer for the mesh.
   * @return Reference to the vertex buffer.
   */
  [[nodiscard]] auto getVertexBuffer() const -> const vk::raii::Buffer &;

  /** @brief Returns the index buffer for the mesh.
   * @return Reference to the index buffer.
   */
  [[nodiscard]] auto getIndexBuffer() const -> const vk::raii::Buffer &;

  /** @brief Gets the binding description for the vertex input.
   * @return The vertex input binding description.
   */
  static auto getVertexBindingDescription() -> vk::VertexInputBindingDescription;

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

private:
  /** @brief Loads the mesh from a file and creates the necessary Vulkan resources.
   * @param uploadContext Upload command context that keeps staging buffers alive.
   */
  auto loadFromFile(UploadContext &uploadContext) -> void;

  Buffer m_vertexBuffer; // Vertex buffer for the mesh
  Buffer m_indexBuffer;  // Index buffer for the mesh

  std::vector<Vertex> vertices;  // Vertices of the mesh
  std::vector<uint32_t> indices; // Indices of the mesh

  std::string m_filePath;   // Path to the mesh file
  VulkanContext *m_context; // Pointer to the Vulkan context for resource management
};
} // namespace vksim