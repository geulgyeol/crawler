add_executable (ImageDownloader "ImageDownloader.cpp")

target_link_libraries(ImageDownloader PUBLIC 
	Library
)

set_target_properties(ImageDownloader PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)