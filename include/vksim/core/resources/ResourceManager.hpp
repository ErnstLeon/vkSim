#pragma once

#include <expected>
#include <string>
#include <typeindex>
#include <unordered_map>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/resources/Resource.hpp"
#include "vksim/core/resources/UploadContext.hpp"

namespace vksim
{
/** @brief Manages the lifecycle of resources in the engine.
 */
class ResourceManager
{
public:
  /** @brief Loads a resource of the specified type and ID. If the resource
   * is already loaded, it returns the existing instance.
   * @tparam T Type of the resource to load.
   * @param resourceId Unique identifier for the resource.
   * @param filepath Path to the resource file.
   * @param uploadContext Upload command context that keeps staging buffers alive.
   * @return Pointer to the loaded resource instance.
   */
  template <typename T>
  auto Load(const std::string &resourceId, const std::string &filepath, VulkanContext &context,
            UploadContext &uploadContext) -> std::expected<T *, std::string>
  {
    auto &typeResources = resources[typeid(T)];
    auto iter = typeResources.find(resourceId);
    if (iter != typeResources.end())
    {
      return std::expected<T *, std::string>(static_cast<T *>(iter->second.get()));
    }

    auto resource = std::make_unique<T>(context.getDevice(), resourceId, filepath);
    if (!resource->Load(uploadContext))
    {
      return std::expected<T *, std::string>(std::unexpect,
                                             "Failed to load resource: " + resourceId);
    }
    T *resourcePtr = resource.get();
    typeResources[resourceId] = std::move(resource);
    return std::expected<T *, std::string>(resourcePtr);
  }

  /** @brief Unloads a resource of the specified type and ID. If the
   * resource is not loaded, it does nothing.
   * @tparam T Type of the resource to unload.
   * @param resourceId Unique identifier for the resource.
   */
  template <typename T>
  auto GetResource(const std::string &resourceId) -> std::expected<T *, std::string>
  {
    auto resourceIt = resources.find(std::type_index(typeid(T)));
    if (resourceIt == resources.end())
    {
      return std::expected<T *, std::string>(std::unexpect, "Resource not found: " + resourceId);
    }

    auto &typeResources = resourceIt->second;
    auto iter = typeResources.find(resourceId);

    if (iter != typeResources.end())
    {
      // Resource found: downcast and return typed pointer
      return std::expected<T *, std::string>(static_cast<T *>(iter->second.get()));
    }

    // Resource not found: return an error message
    return std::expected<T *, std::string>(std::unexpect, "Resource not found: " + resourceId);
  }

  /** @brief Checks if a resource of the specified type and ID is loaded.
   * @tparam T Type of the resource to check.
   * @param resourceId Unique identifier for the resource.
   * @return True if the resource is loaded, false otherwise.
   */
  template <typename T> auto HasResource(const std::string &resourceId) -> bool
  {
    // Efficient existence check without resource access overhead
    auto resourceIt = resources.find(std::type_index(typeid(T)));
    if (resourceIt == resources.end())
    {
      return false;
    }

    return resourceIt->second.contains(resourceId);
  }

  /** @brief Releases a resource of the specified type and ID. If the
   * resource is not loaded, it does nothing.
   * @tparam T Type of the resource to release.
   * @param resourceId Unique identifier for the resource.
   */
  template <typename T> void Release(const std::string &resourceId)
  {
    auto resourceIt = resources.find(std::type_index(typeid(T)));
    if (resourceIt == resources.end())
    {
      return;
    }

    auto &typeResources = resourceIt->second;
    auto typeResourceIt = typeResources.find(resourceId);
    if (typeResourceIt != typeResources.end())
    {
      typeResources.erase(typeResourceIt); // Remove from cache
    }
  }

private:
  /** @brief Maps resource types to their instances. Two-level mapping:
   * first level maps resource type, second level maps resource IDs to
   * their instances.
   */
  std::unordered_map<std::type_index, std::unordered_map<std::string, std::unique_ptr<Resource>>>
      resources;
};
} // namespace vksim