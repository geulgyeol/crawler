add_executable (ProfilePublisher "ProfilePublisher.cpp")

target_link_libraries(ProfilePublisher PUBLIC 
	Library
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ProfilePublisher PROPERTY CXX_STANDARD 17)
endif()

