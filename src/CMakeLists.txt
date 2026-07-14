add_library(vksim SHARED)

target_sources(vksim
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/slang/SlangCompiler.cpp
    ${CMAKE_SOURCE_DIR}/src/core/buffers/Buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/buffers/Image.cpp
    ${CMAKE_SOURCE_DIR}/src/core/device/Device.cpp
    ${CMAKE_SOURCE_DIR}/src/core/context/VulkanContext.cpp
    ${CMAKE_SOURCE_DIR}/src/core/context/DeviceSelector.cpp
    ${CMAKE_SOURCE_DIR}/src/core/context/CommandPool.cpp
    ${CMAKE_SOURCE_DIR}/src/core/scene/Camera.cpp
    ${CMAKE_SOURCE_DIR}/src/core/scene/Light.cpp
    ${CMAKE_SOURCE_DIR}/src/core/scene/Scene.cpp
    ${CMAKE_SOURCE_DIR}/src/core/scene/SceneObject.cpp
    ${CMAKE_SOURCE_DIR}/src/core/resources/UploadContext.cpp
    ${CMAKE_SOURCE_DIR}/src/core/resources/Texture.cpp
    ${CMAKE_SOURCE_DIR}/src/core/resources/Mesh.cpp
    ${CMAKE_SOURCE_DIR}/src/core/resources/Material.cpp
    ${CMAKE_SOURCE_DIR}/src/core/render/Swapchain.cpp
    ${CMAKE_SOURCE_DIR}/src/core/render/Renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/window/Window.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/ImGuiUtil.cpp

  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
      ${CMAKE_SOURCE_DIR}/include
    FILES
      ${CMAKE_SOURCE_DIR}/include/vksim/utility/Logging.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/imgui/ImGuiUtil.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/slang/SlangCompiler.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/buffers/Buffer.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/buffers/Image.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/device/Device.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/context/VulkanContext.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/context/DeviceSelector.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/context/CommandPool.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/scene/Camera.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/scene/Light.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/scene/Scene.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/scene/SceneObject.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/resources/UploadContext.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/resources/ResourceManager.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/resources/Texture.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/resources/Mesh.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/resources/Material.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/render/Swapchain.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/render/Renderer.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/queue/Queue.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/core/window/Window.hpp
)

target_sources(vksim PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp
)

target_include_directories(vksim PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
  ${imguizmo_SOURCE_DIR}/src
)

target_compile_definitions(vksim 
  PRIVATE
  SPDLOG_ACTIVE_LEVEL=${SPDLOG_ACTIVE_LEVEL_OPTION}
  PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(vksim
    PUBLIC
    DEBUG)
endif()

target_link_libraries(vksim 
  PUBLIC 
  Eigen3::Eigen 
  spdlog::spdlog
  Vulkan::Vulkan
  glfw
  glm::glm
  nlohmann_json::nlohmann_json
  slang::slang
  tinyobjloader
  tinygltf
)