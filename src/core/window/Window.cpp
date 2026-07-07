#include "vksim/core/window/Window.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim
{

Window::Window(uint32_t width, uint32_t height) : m_width(width), m_height(height)
{
  if (glfwInit() == 0)
  {
    spdlog::error("Failed to initialize GLFW");
    std::abort();
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  m_window = glfwCreateWindow(m_width, m_height, "VkSim Window", nullptr, nullptr);
  if (m_window == nullptr)
  {
    spdlog::error("Failed to create GLFW window");
    glfwTerminate();
    std::abort();
  }
  glfwSetWindowUserPointer(m_window, this);
  glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
}

void Window::framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
  auto *app = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
  app->m_width = width;
  app->m_height = height;
}

auto Window::getWidth() const -> uint32_t { return m_width; }
auto Window::getHeight() const -> uint32_t { return m_height; }
auto Window::getGLFWwindow() const -> GLFWwindow * { return m_window; }

auto Window::getFramebufferSize(int &width, int &height) const -> void
{
  glfwGetFramebufferSize(m_window, &width, &height);
}

auto Window::waitEvents() -> void { glfwWaitEvents(); }

void Window::pollEvents() { glfwPollEvents(); }

auto Window::shouldClose() const -> bool { return glfwWindowShouldClose(m_window) != 0; }

Window::~Window()
{
  if (m_window != nullptr)
  {
    glfwDestroyWindow(m_window);
    m_window = nullptr;
  }
  glfwTerminate();
}

} // namespace vksim