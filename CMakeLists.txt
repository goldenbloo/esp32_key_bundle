cmake_minimum_required(VERSION 3.16.0)
list(APPEND EXTRA_COMPONENT_DIRS ./lib/u8g2-hal-esp-idf ./lib/u8g2)

get_filename_component(configName "${CMAKE_BINARY_DIR}" NAME)
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/.pio/libdeps/${configName}/esp_littlefs")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_cmake_test)
