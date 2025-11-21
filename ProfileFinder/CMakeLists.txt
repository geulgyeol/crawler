add_executable (ProfileFinder "ProfileFinder.cpp")

target_link_libraries(ProfileFinder PUBLIC 
	Library
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ProfileFinder PROPERTY CXX_STANDARD 17)
endif()

