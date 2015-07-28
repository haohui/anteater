include_directories(${LLVM_INCLUDE_DIR})

set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__STDC_LIMIT_MACROS=1 -D__STDC_CONSTANT_MACROS=1" )

add_executable (ppa-hint-generator main.cpp)
set_target_properties (ppa-hint-generator PROPERTIES
  LINK_FLAGS ${LLVM_LFLAGS})
target_link_libraries (ppa-hint-generator SPATarget SPACore)
target_link_libraries (ppa-hint-generator ${LLVM_LIBS})
