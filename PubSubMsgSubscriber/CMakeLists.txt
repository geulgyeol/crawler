add_executable (PubSubMsgSubscriber "PubSubMsgSubscriber.cpp")

target_link_libraries(PubSubMsgSubscriber PUBLIC 
	Library
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET PubSubMsgSubscriber PROPERTY CXX_STANDARD 17)
endif()

