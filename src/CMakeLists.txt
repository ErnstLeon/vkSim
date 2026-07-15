add_library(vksim SHARED)

target_sources(vksim
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/slang/SlangCompiler.cpp
    ${CMAKE_SOURCE_DIR}/src/render/buffers/Buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/render/buffers/Image.cpp
    ${CMAKE_SOURCE_DIR}/src/render/device/Device.cpp
    ${CMAKE_SOURCE_DIR}/src/render/context/VulkanContext.cpp
    ${CMAKE_SOURCE_DIR}/src/render/context/DeviceSelector.cpp
    ${CMAKE_SOURCE_DIR}/src/render/context/CommandPool.cpp
    ${CMAKE_SOURCE_DIR}/src/render/scene/Camera.cpp
    ${CMAKE_SOURCE_DIR}/src/render/scene/Light.cpp
    ${CMAKE_SOURCE_DIR}/src/render/scene/Scene.cpp
    ${CMAKE_SOURCE_DIR}/src/render/scene/SceneObject.cpp
    ${CMAKE_SOURCE_DIR}/src/render/resources/UploadContext.cpp
    ${CMAKE_SOURCE_DIR}/src/render/resources/Texture.cpp
    ${CMAKE_SOURCE_DIR}/src/render/resources/Mesh.cpp
    ${CMAKE_SOURCE_DIR}/src/render/resources/Material.cpp
    ${CMAKE_SOURCE_DIR}/src/render/render/Swapchain.cpp
    ${CMAKE_SOURCE_DIR}/src/render/render/Renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/render/window/Window.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/ImGuiUtil.cpp

  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
      ${CMAKE_SOURCE_DIR}/include
    FILES
      ${CMAKE_SOURCE_DIR}/include/vksim/utility/Logging.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/imgui/ImGuiUtil.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/slang/SlangCompiler.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/buffers/Buffer.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/buffers/Image.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/device/Device.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/context/VulkanContext.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/context/DeviceSelector.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/context/CommandPool.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/scene/Camera.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/scene/Light.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/scene/Scene.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/scene/SceneObject.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/resources/UploadContext.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/resources/ResourceManager.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/resources/Texture.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/resources/Mesh.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/resources/Material.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/render/Swapchain.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/render/Renderer.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/queue/Queue.hpp
      ${CMAKE_SOURCE_DIR}/include/vksim/render/window/Window.hpp
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