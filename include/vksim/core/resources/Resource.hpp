#pragma once

#include <string>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <utility>
#include <vulkan/vulkan_raii.hpp>

#include "vksim/core/context/VulkanContext.hpp"

namespace vksim
{

/** @brief Base class for all resources in the engine.
 */
class Resource
{
private:
  std::string resourceId; // Unique identifier for this resource within the system
  bool loaded = false;    // Loading state flag for resource lifecycle management

public:
  explicit Resource(std::string ident) : resourceId(std::move(ident)) {}
  Resource() = default;
  virtual ~Resource() = default;

  /** @brief Gets the unique identifier for this resource.
   * @return Reference to the resource ID.
   */
  [[nodiscard]] auto GetId() const -> const std::string & { return resourceId; }

  /** @brief Checks if the resource is currently loaded.
   * @return True if the resource is loaded, false otherwise.
   */
  [[nodiscard]] auto IsLoaded() const -> bool { return loaded; }

  /** @brief Loads the resource.
   * @param context Reference to the Vulkan context for resource creation.
   * @return True if the resource was successfully loaded, false otherwise.
   */
  [[nodiscard]] auto Load() -> bool
  {
    loaded = doLoad();
    return loaded;
  }

protected:
  virtual auto doLoad() -> bool = 0;
};
} // namespace vksim