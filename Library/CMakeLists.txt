add_library(
    Library
    STATIC
    "pch.h" 
    "pch.cpp" 
    "framework.h" 
    "Library.cpp" 
    "config.h" 
    "config.cpp"
)

target_include_directories(Library PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(Library PUBLIC
    unofficial::pulsar::pulsar
    CURL::libcurl
)

set_target_properties(Library PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)