add_executable (RePublisher "RePublisher.cpp")

target_link_libraries(RePublisher PUBLIC 
	Library
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET RePublisher PROPERTY CXX_STANDARD 17)
endif()

