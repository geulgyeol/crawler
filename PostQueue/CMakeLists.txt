add_executable (PostQueue "PostQueue.cpp")

target_link_libraries(PostQueue PUBLIC 
	Library
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET PostQueue PROPERTY CXX_STANDARD 17)
endif()

