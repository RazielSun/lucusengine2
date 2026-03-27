cmake_minimum_required(VERSION 3.28)
project ( liblucus )

if(APPLE OR WIN32)
    set ( CMAKE_CXX_STANDARD 20 )
endif()

add_subdirectory ( "../third-party" third-party )

add_subdirectory ( lucus-core )
add_subdirectory ( lucus-math )
add_subdirectory ( lucus-graphics )
add_subdirectory ( lucus-sim )

add_library ( liblucus INTERFACE)

target_link_libraries ( liblucus INTERFACE
    lucus-sim
    lucus-graphics
    lucus-math
    lucus-core
)