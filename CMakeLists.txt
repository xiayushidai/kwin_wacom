cmake_minimum_required(VERSION 3.13)

project(wacom)

set(TARGET_NAME wacom)

find_package(PkgConfig)
pkg_search_module(LIBINPUT REQUIRED  libinput)
pkg_search_module(LIBUDEV REQUIRED libudev)


file(GLOB SRC_LIST "*.c" "*.h")


add_executable(${TARGET_NAME} ${SRC_LIST})

include_directories(${LIBINPUT_INCLUDE_DIRS})
include_directories(${LIBINPUT_INCLUDE_DIRS})
include_directories("/usr/include/pixman-1/")

target_link_libraries(${TARGET_NAME} ${LIBINPUT_LIBRARIES} ${LIBUDEV_LIBRARIES} dl)
target_compile_options(${TARGET_NAME} PUBLIC ${LIBINPUT_CFLAGS_OTHER})
target_compile_options(${TARGET_NAME} PUBLIC ${LIBUDEV_CFLAGS_OTHER})
