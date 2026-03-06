cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT (UNIX AND NOT APPLE))
  message(FATAL_ERROR "Linux-only build config")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# if (MSVC)
#   target_compile_options(mini_rhi PRIVATE /W4)
# else()
#   target_compile_options(mini_rhi PRIVATE -Wall -Wextra -Wpedantic)
# endif()