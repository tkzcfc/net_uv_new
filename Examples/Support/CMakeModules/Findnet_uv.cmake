
if (TARGET net_uv)
    return()
endif()

set(NET_UV_SRC ${CMAKE_SOURCE_DIR}/../net_uv)
set(_net_uv_BinaryDir ${CMAKE_BINARY_DIR}/../net_uv)

add_subdirectory(${NET_UV_SRC} ${_net_uv_BinaryDir})

include(${CMAKE_ROOT}/Modules/FindPackageHandleStandardArgs.cmake)

find_package_handle_standard_args(
    net_uv
    REQUIRED_VARS
        NET_UV_SRC
)

