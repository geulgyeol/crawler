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

find_package(google_cloud_cpp_pubsub REQUIRED)
find_package(CURL REQUIRED)

target_link_libraries(Library PUBLIC
    google-cloud-cpp::pubsub
    CURL::libcurl
)

target_include_directories(Library PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")