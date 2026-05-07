add_executable (LinkFinder "LinkFinder.cpp")

target_link_libraries(LinkFinder PUBLIC 
	Library
)

set_target_properties(LinkFinder PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)