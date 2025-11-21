add_executable (HTMLCrawler "HTMLCrawler.cpp")

target_link_libraries(HTMLCrawler PUBLIC 
	Library
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HTMLCrawler PROPERTY CXX_STANDARD 20)
endif()

