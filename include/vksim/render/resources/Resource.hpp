#pragma once

#include <string>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <utility>
#include <vulkan/vulkan_raii.hpp>

#include "vksim/render/resources/UploadContext.hpp"

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
   * @return True if the resource was successfully loaded, false otherwise.
   */
  [[nodiscard]] auto Load(UploadContext &uploadContext) -> bool
  {
    loaded = doLoad(uploadContext);
    return loaded;
  }

protected:
  virtual auto doLoad(UploadContext &uploadContext) -> bool = 0;
};
} // namespace vksim