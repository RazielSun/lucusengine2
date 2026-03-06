cmake_minimum_required(VERSION 3.28)
project ( lucus-graphics )

# TODO: fix me
find_package(Vulkan REQUIRED)
add_library(lucus::vulkan ALIAS Vulkan::Vulkan)

set ( MODULE_NAME "lucus-graphics" )
set ( CMAKE_CURRENT_SOURCE_DIR "${LUCUS_ENGINE_SRC}/${MODULE_NAME}" )
set ( MODULE_EXTRA_LIBS "lucus::glfw" "lucus::vulkan" )


set ( MODULE_INCLUDES 
    "${LUCUS_ENGINE_SRC}/${MODULE_NAME}/"
)

# Use all cpp files in directory as source code
file ( GLOB MODULE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp" )

include_directories ( ${MODULE_INCLUDES} )

add_library ( ${MODULE_NAME} STATIC ${MODULE_SRC} )

set( BUILD_INCLUDES
    ${MODULE_INCLUDES}
    ${MODULE_EXTRA_INCLUDES}
)
 
target_include_directories (${MODULE_NAME} PUBLIC 
    "$<BUILD_INTERFACE:${BUILD_INCLUDES}>"
    "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>"
)

# target_precompile_headers ( liblucus
#     PRIVATE
#     "$<$<COMPILE_LANGUAGE:CXX>:${LUCUS_ENGINE_SRC}/pch.hpp>"
# )

target_link_libraries ( ${MODULE_NAME} PUBLIC
    ${MODULE_EXTRA_LIBS} 
)

set_target_properties(${MODULE_NAME} PROPERTIES FOLDER Graphics)  

install(TARGETS ${MODULE_NAME} EXPORT liblucus ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION include/${MODULE_NAME}
          FILES_MATCHING PATTERN "*.hpp")