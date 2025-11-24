add_executable (Function_Test "Function_Test.cpp")

target_link_libraries(Function_Test PUBLIC 
	Library
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Function_Test PROPERTY CXX_STANDARD 17)
endif()

