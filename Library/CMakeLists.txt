add_library(
    Library
    STATIC
    "pch.h" 
    "pch.cpp" 
    "framework.h" 
    "Library.cpp" 
    "config.h" 
    "config.cpp"
    "config.yaml"
)

target_include_directories(Library PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(Library PUBLIC
    unofficial::pulsar::pulsar
    CURL::libcurl
    yaml-cpp::yaml-cpp
)

set_target_properties(Library PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_custom_command(TARGET Library POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/config.yaml"
            "${CMAKE_BINARY_DIR}/config.yaml"
    COMMENT "Copying config.yaml to the executable directory"
)