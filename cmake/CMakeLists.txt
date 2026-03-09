cmake_minimum_required(VERSION 3.28)
project ( app LANGUAGES C CXX)

get_filename_component ( LUCUS_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE )
get_filename_component ( LUCUS_ENGINE_SRC "${LUCUS_ENGINE_ROOT}/src/" ABSOLUTE )
get_filename_component ( LUCUS_ENGINE_APP_SRC "${LUCUS_ENGINE_ROOT}/app/" ABSOLUTE )

set ( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${LUCUS_ENGINE_ROOT}/bin )

add_subdirectory ( common )
add_subdirectory ( liblucus )

file ( GLOB APP_SRC "${LUCUS_ENGINE_APP_SRC}/*.cpp" "${LUCUS_ENGINE_APP_SRC}/*.h" )

add_executable ( app ${APP_SRC} )

target_include_directories ( app PRIVATE
    "${LUCUS_ENGINE_SRC}/lucus-sim/"
)

target_link_libraries ( app PRIVATE liblucus )

install ( TARGETS app DESTINATION bin )