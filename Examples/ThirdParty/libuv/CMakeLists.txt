project(libuv)

if(MSVC)
  set(uv_cflags /W4)
elseif(CMAKE_C_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
  set(uv_cflags
  -fvisibility=hidden
  --std=gnu89
  -Wall 
  -Wextra 
  -Wstrict-prototypes
  -Wno-unused-parameter)
endif()


set(LIBUV_COM_INCLUDE
  include/android-ifaddrs.h
  include/pthread-barrier.h
  include/stdint-msvc2008.h
  include/tree.h
  include/uv-aix.h
  include/uv-bsd.h
  include/uv-darwin.h
  include/uv-errno.h
  include/uv-linux.h
  include/uv-os390.h
  include/uv-posix.h
  include/uv-sunos.h
  include/uv-threadpool.h
  include/uv-unix.h
  include/uv-version.h
  include/uv-win.h
  include/uv.h
  src/heap-inl.h
  src/queue.h
  src/uv-common.h
  src/fs-poll.c
  src/inet.c
  src/threadpool.c
  src/uv-data-getter-setters.c
  src/version.c
  src/uv-common.c
)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")

#set(uv_defines WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0600 BUILDING_UV_SHARED)
set(uv_defines WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0600)

set(uv_libraries
       advapi32
       Iphlpapi
       Psapi
       shell32
       user32
       Userenv
       ws2_32)

set(LIB_SRC
  src/win/async.c
  src/win/atomicops-inl.h
  src/win/core.c
  src/win/detect-wakeup.c
  src/win/dl.c
  src/win/error.c
  src/win/fs-event.c
  src/win/fs.c
  src/win/getaddrinfo.c
  src/win/getnameinfo.c
  src/win/handle-inl.h
  src/win/handle.c
  src/win/internal.h
  src/win/loop-watcher.c
  src/win/pipe.c
  src/win/poll.c
  src/win/process-stdio.c
  src/win/process.c
  src/win/req-inl.h
  src/win/req.c
  src/win/signal.c
  src/win/snprintf.c
  src/win/stream-inl.h
  src/win/stream.c
  src/win/tcp.c
  src/win/thread.c
  src/win/timer.c
  src/win/tty.c
  src/win/udp.c
  src/win/util.c
  src/win/winapi.c
  src/win/winapi.h
  src/win/winsock.c
  src/win/winsock.h)

else()

set(LIB_SRC
  src/unix/aix-common.c
  src/unix/aix.c
  src/unix/android-ifaddrs.c
  src/unix/async.c
  src/unix/atomic-ops.h
  src/unix/bsd-ifaddrs.c
  src/unix/core.c
  src/unix/cygwin.c
  src/unix/darwin-proctitle.c
  src/unix/darwin.c
  src/unix/dl.c
  src/unix/freebsd.c
  src/unix/fs.c
  src/unix/fsevents.c
  src/unix/getaddrinfo.c
  src/unix/getnameinfo.c
  src/unix/ibmi.c
  src/unix/internal.h
  src/unix/kqueue.c
  src/unix/linux-core.c
  src/unix/linux-inotify.c
  src/unix/linux-syscalls.c
  src/unix/linux-syscalls.h
  src/unix/loop-watcher.c
  src/unix/loop.c
  src/unix/netbsd.c
  src/unix/no-fsevents.c
  src/unix/no-proctitle.c
  src/unix/openbsd.c
  src/unix/os390-syscalls.c
  src/unix/os390-syscalls.h
  src/unix/os390.c
  src/unix/pipe.c
  src/unix/poll.c
  src/unix/posix-hrtime.c
  src/unix/posix-poll.c
  src/unix/process.c
  src/unix/procfs-exepath.c
  src/unix/proctitle.c
  src/unix/pthread-fixes.c
  src/unix/signal.c
  src/unix/spinlock.h
  src/unix/stream.c
  src/unix/sunos.c
  src/unix/sysinfo-loadavg.c
  src/unix/sysinfo-memory.c
  src/unix/tcp.c
  src/unix/thread.c
  src/unix/timer.c
  src/unix/tty.c
  src/unix/udp.c)

endif()

include_directories(
  include
  src
)

function(assign_source_group)
    foreach(_source IN ITEMS ${ARGN})
        if (IS_ABSOLUTE "${_source}")
            file(RELATIVE_PATH _source_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${_source}")
        else()
            set(_source_rel "${_source}")
        endif()
        get_filename_component(_source_path "${_source_rel}" PATH)
        string(REPLACE "/" "\\" _source_path_msvc "${_source_path}")
        source_group("${_source_path_msvc}" FILES "${_source}")
    endforeach()
endfunction(assign_source_group)

function(my_add_library)
    foreach(_source IN ITEMS ${ARGN})
        assign_source_group(${_source})
    endforeach()
    add_library(${ARGV})
endfunction(my_add_library)

# STATIC SHARED
my_add_library(${PROJECT_NAME} STATIC
  ${LIBUV_COM_INCLUDE}
  ${LIB_SRC}
)

set(_libuv_SourceDir ${LIBUV_COM_INCLUDE} ${LIB_SRC})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(${PROJECT_NAME} PRIVATE ${uv_defines})
target_compile_options(${PROJECT_NAME} PRIVATE ${uv_cflags})
target_link_libraries(${PROJECT_NAME} ${uv_libraries})

set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "ThirdParty")
