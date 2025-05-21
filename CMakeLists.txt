cmake_minimum_required(VERSION 3.31)

find_package(cmake-bare REQUIRED PATHS node_modules/cmake-bare)
find_package(cmake-fetch REQUIRED PATHS node_modules/cmake-fetch)

project(bare_webp C)

fetch_package("github:webmproject/libwebp@1.5.0")

add_bare_module(bare_webp)

target_sources(
  ${bare_webp}
  PRIVATE
    binding.c
)

target_link_libraries(
  ${bare_webp}
  PUBLIC
    webp
)
