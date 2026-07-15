#include <cstddef>
#include <cstring>

#include "vksim/render/resources/Material.hpp"
#include <spdlog/spdlog.h>

namespace vksim
{

Material::Material(Device &device, const std::string &identifier, const MaterialInfo &properties)
    : Resource(identifier), m_material(properties), m_materialUniformBuffer(device),
      m_device(device)
{
}

auto Material::getBaseColor() const -> const glm::vec3 & { return m_material.m_baseColor; }

auto Material::getMetallic() const -> float { return m_material.m_metallic; }

auto Material::getRoughness() const -> float { return m_material.m_roughness; }

auto Material::doLoad(UploadContext &uploadContext) -> bool
{
  // Create a staging buffer for the material data
  auto stagingBuffer = Buffer(m_device);
  stagingBuffer.create(BufferCreateInfo{.size = sizeof(MaterialInfo),
                                        .usage = vk::BufferUsageFlagBits::eTransferSrc,
                                        .properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                                      vk::MemoryPropertyFlagBits::eHostCoherent});

  // Map the staging buffer and copy the material data into it
  void *mapped = stagingBuffer.getVkBufferMemory().mapMemory(0, sizeof(MaterialInfo));
  std::memcpy(mapped, &m_material, sizeof(MaterialInfo));
  stagingBuffer.getVkBufferMemory().unmapMemory();

  // Create the material uniform buffer on the GPU with device-local memory
  m_materialUniformBuffer.create(BufferCreateInfo{
      .size = sizeof(MaterialInfo),
      .usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
      .properties = vk::MemoryPropertyFlagBits::eDeviceLocal});

  // Copy the data from the staging buffer to the GPU buffer using the provided command buffer
  m_materialUniformBuffer.copyFromBuffer(stagingBuffer, sizeof(MaterialInfo),
                                         uploadContext.getCommandBuffer());

  // Add the staging buffers to the UploadContext to ensure they remain alive until the upload is
  // complete
  uploadContext.addStagingBuffer(std::move(stagingBuffer));

  spdlog::info("Material {} loaded successfully", GetId());
  return true;
}

} // namespace vksim
