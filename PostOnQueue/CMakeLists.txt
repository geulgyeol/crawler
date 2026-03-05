add_executable (PostOnQueue "PostOnQueue.cpp")

target_link_libraries(PostOnQueue PUBLIC 
	Library
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET PostOnQueue PROPERTY CXX_STANDARD 17)
endif()

