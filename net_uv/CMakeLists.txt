project(net_uv)

set(NET_UV_BASE_SRC
  base/Buffer.h
  base/Client.cpp
  base/Client.h
  base/Common.cpp
  base/Common.h
  base/Config.h
  base/DNSCache.cpp
  base/DNSCache.h
  base/Macros.h
  base/Misc.cpp
  base/Misc.h
  base/Mutex.cpp
  base/Mutex.h
  base/Runnable.cpp
  base/Runnable.h
  base/Server.cpp
  base/Server.h
  base/Session.cpp
  base/Session.h
  base/Loop.cpp
  base/Loop.h
  base/SessionManager.cpp
  base/SessionManager.h
  base/Socket.cpp
  base/Socket.h
  base/md5.cpp
  base/md5.h
)

set(NET_UV_COMMON_SRC
  common/NetUVThreadMsg.h
)

set(NET_UV_HTTP_SRC
    http/HttpCommon.h
    http/HttpContext.cpp
    http/HttpContext.h
    http/HttpDetail.cpp
    http/HttpDetail.h
    http/HttpRequest.cpp
    http/HttpRequest.h
    http/HttpResponse.cpp
    http/HttpResponse.h
    http/HttpServer.cpp
    http/HttpServer.h
)

set(NET_UV_KCP_SRC
    kcp/ikcp.c
    kcp/ikcp.h
    kcp/KCPClient.cpp
    kcp/KCPClient.h
    kcp/KCPCommon.h
    kcp/KCPConfig.h
    kcp/KCPServer.cpp
    kcp/KCPServer.h
    kcp/KCPSession.cpp
    kcp/KCPSession.h
    kcp/KCPSocket.cpp
    kcp/KCPSocket.h
    kcp/KCPSocketManager.cpp
    kcp/KCPSocketManager.h
    kcp/KCPUtils.cpp
    kcp/KCPUtils.h
)

set(NET_UV_P2P_SRC
    p2p/P2PCommon.h
    p2p/P2PMessage.h
    p2p/P2PPeer.cpp
    p2p/P2PPeer.h
    p2p/P2PPipe.cpp
    p2p/P2PPipe.h
    p2p/P2PTurn.cpp
    p2p/P2PTurn.h
)

set(NET_UV_TCP_SRC
    tcp/TCPClient.cpp
    tcp/TCPClient.h
    tcp/TCPCommon.h
    tcp/TCPConfig.h
    tcp/TCPServer.cpp
    tcp/TCPServer.h
    tcp/TCPSession.cpp
    tcp/TCPSession.h
    tcp/TCPSocket.cpp
    tcp/TCPSocket.h
)


set(NET_UV_UDP_SRC
    udp/UDPSocket.cpp
    udp/UDPSocket.h
)

set(NET_UV_NET_MSG_MGR_SRC
    msg/NetMsgMgr.cpp
    msg/NetMsgMgr.h
    msg/NetMsgUtils.cpp
    msg/NetMsgUtils.h
)

set(NET_UV_SRC
    net_uv.h
    ${NET_UV_BASE_SRC}
    ${NET_UV_COMMON_SRC}
    ${NET_UV_HTTP_SRC}
    ${NET_UV_KCP_SRC}
    ${NET_UV_P2P_SRC}
	${NET_UV_TCP_SRC}
    ${NET_UV_UDP_SRC}
    ${NET_UV_NET_MSG_MGR_SRC}
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
  ${NET_UV_SRC}
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE libuv)

set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "ThirdParty")

