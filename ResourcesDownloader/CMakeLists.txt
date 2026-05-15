add_executable (ResourcesDownloader "ResourcesDownloader.cpp")

target_link_libraries(ResourcesDownloader PUBLIC 
	Library
)

set_target_properties(ResourcesDownloader PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)