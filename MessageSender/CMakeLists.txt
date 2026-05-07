add_executable (MessageSender "MessageSender.cpp")

target_link_libraries(MessageSender PUBLIC 
	Library
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MessageSender PROPERTY CXX_STANDARD 17)
endif()

