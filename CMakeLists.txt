cmake_minimum_required(VERSION 3.26)
project(VkSim VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Get Eigen
FetchContent_Declare(
  eigen
  GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
  GIT_TAG 5.0.1
)
FetchContent_MakeAvailable(eigen)

# Get pybin11
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.11.1
)

FetchContent_MakeAvailable(pybind11)

# Get SPDLOG
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(spdlog)

# Get glm
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
)
FetchContent_MakeAvailable(glm)

# Get tinyobjloader
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v2.0.0rc13
)
FetchContent_MakeAvailable(tinyobjloader)

# Get stb
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
)
FetchContent_MakeAvailable(stb)

# Get tinygltf
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG v3.0.0
)
FetchContent_MakeAvailable(tinygltf)

# Get nlohmann_json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

# Get imgui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.3
)
FetchContent_MakeAvailable(imgui)

# Get glfw
find_package(glfw3 CONFIG REQUIRED)

# Get Vulkan
find_package(Vulkan REQUIRED)

# Get slang
find_package(slang REQUIRED)

# Set log level depending on build type
if(NOT DEFINED SPDLOG_ACTIVE_LEVEL_OPTION)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(SPDLOG_ACTIVE_LEVEL_OPTION SPDLOG_LEVEL_TRACE)
    else()
        set(SPDLOG_ACTIVE_LEVEL_OPTION SPDLOG_LEVEL_INFO)
    endif()
endif()

# Set doxygen configuration
find_package(Doxygen)
if(DOXYGEN_FOUND)

    set(PROJECT_DESCRIPTION "vksim")
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
        ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        @ONLY
    )

    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        COMMENT "Generating API documentation with Doxygen"
        BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/docs/html/index.html
        VERBATIM
    )
else()
    message(WARNING "Doxygen not found. Documentation target will be disabled.")
endif()

# Add Subdirectories
add_subdirectory(src)
add_subdirectory(examples)
