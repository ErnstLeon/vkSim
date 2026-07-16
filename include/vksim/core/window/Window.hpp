#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace vksim
{
/** @brief A wrapper around a GLFW window with Vulkan support
 * @note Window is supposed to be a singleton, as there should only be one window in the
 * application. The destructor of Window will automatically terminate GLFW when the window is
 * destroyed. Therefore, it is recommended to create a single instance of Window at the start of the
 * application and use it throughout the application's lifetime.
 */
class Window
{
public:
  Window() = default;
  Window(uint32_t width, uint32_t height);

  Window(const Window &) = delete;
  Window(Window &&) noexcept = delete;

  auto operator=(const Window &) -> Window & = delete;
  auto operator=(Window &&) -> Window & = delete;

  ~Window();

  [[nodiscard]] auto getWidth() const -> uint32_t;
  [[nodiscard]] auto getHeight() const -> uint32_t;
  [[nodiscard]] auto getGLFWwindow() const -> GLFWwindow *;

  auto getFramebufferSize(int &width, int &height) const -> void;

  static auto waitEvents() -> void;

  static void pollEvents();
  [[nodiscard]] auto shouldClose() const -> bool;

private:
  static void framebufferResizeCallback(GLFWwindow *window, int width, int height);

  uint32_t m_width{};
  uint32_t m_height{};
  GLFWwindow *m_window = nullptr;
};
} // namespace vksim