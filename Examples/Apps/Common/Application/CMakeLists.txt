
set(_Application_Sources
    Include/Application.h
	Include/json.hpp
    Include/help/Helper.h
    Include/log/Logger.h
    Include/texture/TextureCache.h
)


#find_package(glfw 3 REQUIRED)
find_package(OpenGL REQUIRED)

list(APPEND _Application_Sources
	Source/GLFW/Entry.cpp
	Source/GLFW/imgui_impl_glfw_gl3.cpp
	Source/GLFW/imgui_impl_glfw_gl3.h
	Source/help/Helper.cpp
    Source/log/Logger.cpp
    Source/texture/TextureCache.cpp
)



source_group("" FILES ${_Application_Sources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${_Application_Sources})

add_library(Application STATIC ${_Application_Sources})

target_include_directories(Application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Include)

find_package(imgui REQUIRED)
find_package(stb_image REQUIRED)
target_link_libraries(Application PUBLIC imgui)
target_link_libraries(Application PRIVATE stb_image)

target_include_directories(Application PRIVATE ${OPENGL_INCLUDE_DIR})

find_package(gl3w REQUIRED)
target_link_libraries(Application PRIVATE ${OPENGL_gl_LIBRARY} gl3w glfw)

if (APPLE)
    target_link_libraries(Application PRIVATE
        "-framework CoreFoundation"
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework IOKit"
        "-framework CoreVideo"
    )
endif()

set_property(TARGET Application PROPERTY FOLDER "Apps/Common")
