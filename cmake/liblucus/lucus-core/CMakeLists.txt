cmake_minimum_required(VERSION 3.28)
project ( lucus-core )

set ( MODULE_NAME "lucus-core" )
set ( CMAKE_CURRENT_SOURCE_DIR "${LUCUS_ENGINE_SRC}/${MODULE_NAME}" )
get_target_property (MODULE_EXTRA_INCLUDES lucus::angelscript INCLUDE_DIRECTORIES)
get_target_property (MODULE_EXTRA_INCLUDES lucus::angelscript_addons INCLUDE_DIRECTORIES)
set (MODULE_EXTRA_LIBS "lucus::angelscript" "lucus::angelscript_addons")


set ( MODULE_INCLUDES 
    "${LUCUS_ENGINE_SRC}/lucus-pch/"
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

# target_precompile_headers ( ${MODULE_NAME}
#     PRIVATE
#     "$<$<COMPILE_LANGUAGE:CXX>:${LUCUS_ENGINE_SRC}/lucus-pch/pch.hpp>"
# )

if(APPLE)
    target_compile_features ( ${MODULE_NAME} PUBLIC cxx_std_20 )
endif()

target_link_libraries ( ${MODULE_NAME} PUBLIC
    ${MODULE_EXTRA_LIBS}
)

set_target_properties(${MODULE_NAME} PROPERTIES FOLDER Core)  

install(TARGETS ${MODULE_NAME} EXPORT liblucus ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION include/${MODULE_NAME}
          FILES_MATCHING PATTERN "*.hpp")