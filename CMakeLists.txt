cmake_minimum_required (VERSION 3.8)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("GeulgyeolCrawler")

add_subdirectory ("Library")
add_subdirectory ("LinkFinder")
add_subdirectory ("ProfileFinder")
add_subdirectory ("HTMLCrawler")
add_subdirectory ("ProfilePublisher")
add_subdirectory ("Function_Test")