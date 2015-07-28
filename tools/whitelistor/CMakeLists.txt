include_directories(${LLVM_INCLUDE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/lib/Core)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__STDC_LIMIT_MACROS=1 -D__STDC_CONSTANT_MACROS=1")

add_executable (whitelistor whitelistor.cpp)
set_target_properties (whitelistor PROPERTIES LINK_FLAGS ${LLVM_LFLAGS})
target_link_libraries (whitelistor SPACore ${LLVM_LIBS})
