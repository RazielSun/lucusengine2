cmake_minimum_required(VERSION 3.28)
project ( liblucus )

add_subdirectory ( "../third-party" third-party )

add_subdirectory ( lucus-graphics )
add_subdirectory ( lucus-core )

add_library ( liblucus INTERFACE)

target_link_libraries ( liblucus INTERFACE
    lucus-core
    lucus-graphics
)