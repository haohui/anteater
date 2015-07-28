include_directories(${LLVM_INCLUDE_DIR})
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__STDC_LIMIT_MACROS=1 -D__STDC_CONSTANT_MACROS=1" )

add_library (SPATransform SHARED BareGVN.cpp EliminateRedundantPrecondition.cpp
 MergePacket.cpp PacketScalarReplacement.cpp RemoveAnnotation.cpp)

target_link_libraries (SPATransform SPACore)

if (APPLE)
set_target_properties (SPATransform PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
else (APPLE)
set_target_properties (SPATransform PROPERTIES LINK_FLAGS ${LLVM_LFLAGS})
endif(APPLE)
