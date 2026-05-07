add_executable (Function_Test "Function_Test.cpp")

target_link_libraries(Function_Test PUBLIC 
    Library
)

set_target_properties(Function_Test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)