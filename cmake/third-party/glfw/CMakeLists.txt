cmake_minimum_required(VERSION 3.28)
# project ( glfw )

set ( CMAKE_CURRENT_SOURCE_DIR "${LUCUS_ENGINE_ROOT}/third-party/glfw/" )

set(GLFW_BIN_DIR "${CMAKE_BINARY_DIR}/third-party/glfw")

# set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
# set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}" "${GLFW_BIN_DIR}" EXCLUDE_FROM_ALL)
add_library(lucus::glfw ALIAS glfw)

set_target_properties ( glfw PROPERTIES FOLDER Third-Party )

install(TARGETS glfw EXPORT liblucus ARCHIVE DESTINATION lib)