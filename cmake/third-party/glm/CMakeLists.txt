cmake_minimum_required(VERSION 3.28)
# project ( glm )

set ( CMAKE_CURRENT_SOURCE_DIR "${LUCUS_ENGINE_ROOT}/third-party/glm/" )

set(GLM_BIN_DIR "${CMAKE_BINARY_DIR}/third-party/glm")

set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}" "${GLM_BIN_DIR}" EXCLUDE_FROM_ALL)

add_library(lucus_glm INTERFACE)
target_link_libraries(lucus_glm INTERFACE glm::glm)

add_library(lucus::glm ALIAS lucus_glm)

# set_target_properties ( glm PROPERTIES FOLDER Third-Party )

# install(TARGETS glm EXPORT liblucus ARCHIVE DESTINATION lib)