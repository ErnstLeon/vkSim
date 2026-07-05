add_executable(example test.cpp)

target_link_libraries(example PRIVATE vksim)

target_compile_definitions(example 
  PRIVATE
  SPDLOG_ACTIVE_LEVEL=${SPDLOG_ACTIVE_LEVEL_OPTION}
  PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)