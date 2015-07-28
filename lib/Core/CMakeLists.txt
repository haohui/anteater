include_directories(${LLVM_INCLUDE_DIR})
# Linked with Transformation
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__STDC_LIMIT_MACROS=1 -D__STDC_CONSTANT_MACROS=1 -fPIC" )

add_library (SPACore STATIC IRBuilder.cpp SymbolicPacket.cpp)

add_library (SPACoreShared SHARED IRBuilder.cpp SymbolicPacket.cpp)
target_link_libraries (SPACoreShared ${LLVM_LIBS})
set_target_properties (SPACoreShared PROPERTIES LINK_FLAGS ${LLVM_LFLAGS})

