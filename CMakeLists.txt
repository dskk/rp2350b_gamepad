cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(minimal_hid_project C CXX ASM)

pico_sdk_init()

add_executable(minimal_hid src/main.c)

# include ディレクトリの指定
target_include_directories(minimal_hid PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 必要な Pico SDK のハードウェアライブラリをリンク
target_link_libraries(minimal_hid
    pico_stdlib
    hardware_irq
    hardware_resets
    pico_multicore
    hardware_flash
)

# uf2 ファイル等の生成を指示
pico_add_extra_outputs(minimal_hid)
