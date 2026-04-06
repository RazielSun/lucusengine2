cmake_minimum_required(VERSION 3.28)

set(TINYGLTF_ROOT "${LUCUS_ENGINE_ROOT}/third-party/tinygltf")

add_library(lucus_tinygltf INTERFACE)

target_include_directories(lucus_tinygltf INTERFACE
    "$<BUILD_INTERFACE:${TINYGLTF_ROOT}>"
    "$<INSTALL_INTERFACE:include/third-party/tinygltf>"
)

add_library(lucus::tinygltf ALIAS lucus_tinygltf)

set_target_properties(lucus_tinygltf PROPERTIES FOLDER Third-Party)
