cmake_minimum_required(VERSION 3.12)

# 设置conan生成的cmake文件的路径
set(CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/Debug/generated")

find_package(libbpf CONFIG REQUIRED)
find_package(elfutils CONFIG REQUIRED)
find_package(ZLIB CONFIG REQUIRED)

add_executable(TestNetEbpf
    TestNetEbpf.cc
)

target_link_libraries(TestNetEbpf
    PRIVATE
        libbpf::libbpf
        elfutils::elfutils
        ZLIB::ZLIB
)