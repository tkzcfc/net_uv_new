add_example(Control
    main.cpp
    Exception.h
    Exception.cpp
)

#https://www.cnblogs.com/dverdon/p/4574221.html
#add_custom_command(
#    TARGET CocosConsole
#    PRE_BUILD
#	COMMAND ${CMAKE_COMMAND} -E make_directory ARGS ${_ExampleBinDir}/CocosConsole
#    COMMAND ${CMAKE_COMMAND} -E copy_directory ARGS ${CMAKE_SOURCE_DIR}/Apps/CocosConsole/Data ${_ExampleBinDir}/CocosConsole/Data
#)

add_custom_target(
	COMMAND ${CMAKE_COMMAND} -E make_directory ARGS ${_ExampleBinDir}/Control
)

