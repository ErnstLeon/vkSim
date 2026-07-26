#include <cstddef>
#include <cstring>

#include "vksim/core/resources/Material.hpp"
#include <spdlog/spdlog.h>

namespace vksim
{

Material::Material(VulkanContext &context, const std::string &identifier,
                   const MaterialInfo &properties)
    : Resource(identifier), m_material(properties), m_materialUniformBuffer(context),
      m_context(context)
{
}

auto Material::getBaseColor() const -> const glm::vec3 & { return m_material.m_baseColor; }

auto Material::getMetallic() const -> float { return m_material.m_metallic; }

auto Material::getRoughness() const -> float { return m_material.m_roughness; }

auto Material::doLoad() -> bool
{
  // Create the material uniform buffer on the GPU with device-local memory
  m_materialUniformBuffer.create(BufferCreateInfo{
      .size = sizeof(MaterialInfo),
      .usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
      .properties = vk::MemoryPropertyFlagBits::eDeviceLocal});

  // Copy the material data to the GPU buffer using a staging buffer
  m_materialUniformBuffer.copyFromHost(&m_material, sizeof(MaterialInfo));

  spdlog::info("Material {} loaded successfully", GetId());
  return true;
}

} // namespace vksim
