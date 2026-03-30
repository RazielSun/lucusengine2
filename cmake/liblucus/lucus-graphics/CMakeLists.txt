cmake_minimum_required(VERSION 3.28)
project ( lucus-graphics )

if(APPLE)
    enable_language(OBJC OBJCXX)
endif()

set ( MODULE_NAME "lucus-graphics" )
set ( CMAKE_CURRENT_SOURCE_DIR "${LUCUS_ENGINE_SRC}/${MODULE_NAME}" )
set ( MODULE_EXTRA_INCLUDES "" )
foreach(t lucus-math)
    get_target_property(_inc ${t} INCLUDE_DIRECTORIES)
    if(_inc)
        list(APPEND MODULE_EXTRA_INCLUDES ${_inc})
    endif()
endforeach()
set ( MODULE_EXTRA_LIBS "lucus::glfw" )

if (WIN32)
    list(APPEND MODULE_EXTRA_LIBS d3d12 dxgi dxguid d3dcompiler)
elseif(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    list(APPEND MODULE_EXTRA_INCLUDES "${LUCUS_ENGINE_ROOT}/third-party/directx")
    list(APPEND MODULE_EXTRA_LIBS ${COCOA_FRAMEWORK} ${QUARTZCORE_FRAMEWORK} ${METAL_FRAMEWORK} ${COREGRAPHICS_FRAMEWORK})
elseif(UNIX)
    find_package(Vulkan REQUIRED)
    list(APPEND MODULE_EXTRA_LIBS Vulkan::Vulkan)
endif()

set ( MODULE_INCLUDES 
    "${LUCUS_ENGINE_SRC}/lucus-pch/"
    "${LUCUS_ENGINE_SRC}/${MODULE_NAME}/"
)

# Use all cpp files in directory as source code
file ( GLOB MODULE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp" )

# platform specific
if (WIN32)
    file(GLOB PLATFORM_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/dx12/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/dx12/*.hpp"
    )

elseif(APPLE)
    file(GLOB PLATFORM_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/metal/*.mm"
        "${CMAKE_CURRENT_SOURCE_DIR}/metal/*.hpp"
    )

elseif(UNIX)
    file(GLOB PLATFORM_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/*.hpp"
    )
endif()

# merge
list(APPEND MODULE_SRC ${PLATFORM_SRC})

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

# target_precompile_headers ( ${MODULE_NAME}
#     PRIVATE
#     "$<$<COMPILE_LANGUAGE:CXX>:${LUCUS_ENGINE_SRC}/lucus-pch/pch.hpp>"
# )

target_link_libraries ( ${MODULE_NAME} PUBLIC
    ${MODULE_EXTRA_LIBS} 
)

set_target_properties(${MODULE_NAME} PROPERTIES FOLDER Graphics)  

install(TARGETS ${MODULE_NAME} EXPORT liblucus ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION include/${MODULE_NAME}
          FILES_MATCHING PATTERN "*.hpp")