#pragma once

#include <string>

namespace vksim::error
{
/** @brief Enumeration of possible error codes in the vksim project. */
enum class ErrorCode : uint8_t
{
  // Shader compilation errors
  SlangCompilationFailed,
  // Vulkan-related errors
  UnsupportedLayer,
  UnsupportedExtension,
  VulkanError,
  // Validation layer errors
  VulkanValidationError
};

/** @brief Struct representing an error with a code and an optional
 * message.
 * The Error struct encapsulates an error code and an optional message
 * providing additional details about the error. It also includes a method
 * to convert the error to a string representation for easier logging and
 * debugging.
 */
struct Error
{
  ErrorCode code;
  std::string message;

  /** @brief Constructs an Error with the given code and message.
   *
   * @param code The error code.
   * @param message An optional message providing additional details about
   * the error (default is an empty string).
   */
  Error(ErrorCode code, std::string message = "") : code(code), message(std::move(message)) {}

  /** @brief Converts the error to a string representation.
   * @return A string describing the error, including the error code and
   * any additional message.
   */
  [[nodiscard]] auto toString() const -> std::string
  {
    switch (code)
    {
    case ErrorCode::SlangCompilationFailed:
      return "Slang compilation failed: " + message;
    case ErrorCode::UnsupportedLayer:
      return "Unsupported Vulkan layer: " + message;
    case ErrorCode::UnsupportedExtension:
      return "Unsupported Vulkan extension: " + message;
    case ErrorCode::VulkanError:
      return "Vulkan error: " + message;
    case ErrorCode::VulkanValidationError:
      return "Vulkan validation error: " + message;
    default:
      return "Unknown error";
    }
  }
};

} // namespace vksim::error