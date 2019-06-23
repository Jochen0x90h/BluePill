cmake_minimum_required(VERSION 3.9)

# set project
project(host)

# add custom module finders
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

#set(CMAKE_VERBOSE_MAKEFILE ON)

# use C++14 standard by default
set(CMAKE_CXX_STANDARD 14)

find_package(LibUSB REQUIRED)

include_directories(${LIBUSB_INCLUDE_DIR})

add_executable(host
	main.cpp
)
target_link_libraries(host
	${LIBUSB_LIBRARY}
)

if(APPLE)
	target_link_libraries(host "-framework CoreFoundation" "-framework IOKit")
	set_target_properties(host PROPERTIES LINK_FLAGS "-Wl,-F/Library/Frameworks")
endif()
