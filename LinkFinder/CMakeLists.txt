add_executable (LinkFinder "LinkFinder.cpp")

target_link_libraries(LinkFinder PUBLIC 
	Library
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET LinkFinder PROPERTY CXX_STANDARD 17)
endif()

