find_package(Protobuf REQUIRED)
find_package(c-ares REQUIRED)
find_package(gRPC REQUIRED)

function(get_conan_package_dir package_name output_var)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" config_name)

    if(DEFINED ${package_name}_PACKAGE_FOLDER_${config_name})
        set(${output_var} "${${package_name}_PACKAGE_FOLDER_${config_name}}" PARENT_SCOPE)
    elseif(DEFINED ${package_name}_PACKAGE_FOLDER_DEBUG)
        set(${output_var} "${${package_name}_PACKAGE_FOLDER_DEBUG}" PARENT_SCOPE)
    elseif(DEFINED ${package_name}_PACKAGE_FOLDER_RELEASE)
        set(${output_var} "${${package_name}_PACKAGE_FOLDER_RELEASE}" PARENT_SCOPE)
    endif()
endfunction()

function(use_packaged_tool_from_package_dir package_dir tool_name cache_var imported_target)
    if(NOT package_dir)
        return()
    endif()

    set(package_tool "${package_dir}/bin/${tool_name}${CMAKE_EXECUTABLE_SUFFIX}")

    if(EXISTS "${package_tool}")
        set(${cache_var} "${package_tool}" CACHE FILEPATH "Packaged ${tool_name}" FORCE)
        if(TARGET ${imported_target})
            set_property(TARGET ${imported_target} PROPERTY IMPORTED_LOCATION "${package_tool}")
        endif()
    endif()
endfunction()

get_conan_package_dir(protobuf protobuf_PACKAGE_DIR)
get_conan_package_dir(grpc grpc_PACKAGE_DIR)
use_packaged_tool_from_package_dir("${protobuf_PACKAGE_DIR}" protoc
                                   Protobuf_PROTOC_EXECUTABLE protobuf::protoc)
use_packaged_tool_from_package_dir("${grpc_PACKAGE_DIR}" grpc_cpp_plugin
                                   GRPC_CPP_PLUGIN_PROGRAM gRPC::grpc_cpp_plugin)
# find_package(Threads)
#
# Protobuf/Grpc source files
#
set(PROTO_FILES
    monitor_info.proto
    cpu_load.proto
    cpu_softirq.proto
    cpu_stat.proto
    mem_info.proto
    net_info.proto
    disk_info.proto
    query_api.proto
)

add_library(monitor_proto ${PROTO_FILES})    # 生成 monitor_proto 静态库

# 根据找到的包配置链接库
target_link_libraries(monitor_proto PUBLIC protobuf::libprotobuf)
target_link_libraries(monitor_proto PUBLIC gRPC::grpc gRPC::grpc++)
target_include_directories(monitor_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})     # 生成的代码路径（build/proto/）

#
# Compile protobuf and grpc files in mointor proto target to cpp
#
if(TARGET gRPC::grpc_cpp_plugin)
    get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
else()
    set(grpc_cpp_plugin_location ${GRPC_CPP_PLUGIN})
endif()
protobuf_generate(TARGET monitor_proto LANGUAGE cpp)   # 生成 Protobuf 序列化代码
# 生成 gRPC 服务代码
protobuf_generate(TARGET monitor_proto LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")
