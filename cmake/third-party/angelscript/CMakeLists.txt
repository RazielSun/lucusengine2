cmake_minimum_required(VERSION 3.28)
# project ( angelscript )

set ( ANGELSCRIPT_SDK_ROOT "${LUCUS_ENGINE_ROOT}/third-party/angelscript/sdk" )
set ( ANGELSCRIPT_ADDON_ROOT "${ANGELSCRIPT_SDK_ROOT}/add_on" )
set ( ANGELSCRIPT_INCLUDE_ROOT "${ANGELSCRIPT_SDK_ROOT}/angelscript/include" )

set ( CMAKE_CURRENT_SOURCE_DIR "${ANGELSCRIPT_SDK_ROOT}/angelscript/projects/cmake" )

set ( ANGELSCRIPT_BIN_DIR "${CMAKE_BINARY_DIR}/third-party/angelscript" )

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}" "${ANGELSCRIPT_BIN_DIR}" EXCLUDE_FROM_ALL)
add_library(lucus::angelscript ALIAS angelscript)

set_target_properties ( angelscript PROPERTIES FOLDER Third-Party )

install(TARGETS angelscript EXPORT liblucus ARCHIVE DESTINATION lib)

# ADDONS
set ( ADDONS_INCLUDES 
    "${ANGELSCRIPT_ADDON_ROOT}/scriptbuilder/"
    "${ANGELSCRIPT_ADDON_ROOT}/scriptstdstring/"
)

# Use all cpp files in directory as source code
file ( GLOB ADDONS_SRC
    "${ANGELSCRIPT_ADDON_ROOT}/scriptbuilder/*.cpp"
    "${ANGELSCRIPT_ADDON_ROOT}/scriptstdstring/*.cpp"
    "${ANGELSCRIPT_ADDON_ROOT}/scriptbuilder/*.h"
    "${ANGELSCRIPT_ADDON_ROOT}/scriptstdstring/*.h"
)

include_directories ( ${ADDONS_INCLUDES} )

add_library ( angelscript_addons STATIC ${ADDONS_SRC} )
add_library ( lucus::angelscript_addons ALIAS angelscript_addons )

target_include_directories(angelscript_addons
    PUBLIC
        "${ANGELSCRIPT_INCLUDE_ROOT}"
        "${ANGELSCRIPT_ADDON_ROOT}"
)

target_link_libraries(angelscript_addons PUBLIC angelscript )

set_target_properties(angelscript_addons PROPERTIES FOLDER Third-Party)

install(TARGETS angelscript_addons EXPORT liblucus ARCHIVE DESTINATION lib)