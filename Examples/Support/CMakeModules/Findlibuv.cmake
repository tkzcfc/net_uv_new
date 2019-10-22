
if (TARGET libuv)
    return()
endif()

set(_libuv_SourceDir ${CMAKE_SOURCE_DIR}/ThirdParty/libuv)
set(_libuv_BinaryDir ${CMAKE_BINARY_DIR}/ThirdParty/libuv)

add_subdirectory(${_libuv_SourceDir} ${_libuv_BinaryDir})

include(${CMAKE_ROOT}/Modules/FindPackageHandleStandardArgs.cmake)

find_package_handle_standard_args(
    libuv
    REQUIRED_VARS
        _libuv_SourceDir
)

