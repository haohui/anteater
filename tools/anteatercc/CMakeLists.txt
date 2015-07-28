include_directories(${LLVM_INCLUDE_DIR})

set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__STDC_LIMIT_MACROS=1 -D__STDC_CONSTANT_MACROS=1" )

add_executable (anteatercc anteatercc.cpp)
set_target_properties (anteatercc PROPERTIES LINK_FLAGS ${LLVM_LFLAGS})
target_link_libraries (anteatercc SPATarget SPACore SPATransform)
target_link_libraries (anteatercc ${LLVM_LIBS})
