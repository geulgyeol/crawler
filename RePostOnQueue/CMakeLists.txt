add_executable (RePostOnQueue "RePostOnQueue.cpp")

target_link_libraries(RePostOnQueue PUBLIC 
	Library
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET RePostOnQueue PROPERTY CXX_STANDARD 17)
endif()

