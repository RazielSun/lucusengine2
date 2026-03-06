cmake_minimum_required(VERSION 3.28)
# project ( vulkan )

find_package(Vulkan REQUIRED)
add_library(lucus::vulkan ALIAS Vulkan::Vulkan)

set_target_properties ( Vulkan::Vulkan PROPERTIES FOLDER Third-Party )

# Don't install it
# install(TARGETS Vulkan::Vulkan EXPORT liblucus ARCHIVE DESTINATION lib)

if(NOT TARGET lucus::vulkan)
  message(FATAL_ERROR "third-party configured, but lucus::vulkan was NOT created")
endif()