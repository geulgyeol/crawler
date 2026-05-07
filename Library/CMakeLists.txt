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

target_include_directories(Library PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${VCPKG_ROOT}/include"
)

target_link_libraries(Library PUBLIC
    $<$<CONFIG:Debug>:"${VCPKG_ROOT}/debug/lib/pulsar">
    $<$<CONFIG:Release>:"${VCPKG_ROOT}/lib/pulsar">
    $<$<CONFIG:Debug>:"${VCPKG_ROOT}/debug/lib/libcurl-d">
    $<$<CONFIG:Release>:"${VCPKG_ROOT}/lib/libcurl">
)

set_target_properties(Library PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)