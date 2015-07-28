MACRO(ANTEATER_CSVGRAPH_TEST_CASES executable name file)
  add_test(${name} ${RUBY_EXECUTABLE} -I${PROJECT_BINARY_DIR}/lib/Ruby
    "${CMAKE_CURRENT_SOURCE_DIR}/${executable}.rb"
    "${CMAKE_CURRENT_SOURCE_DIR}/${file}"
    )
ENDMACRO(ANTEATER_CSVGRAPH_TEST_CASES)

ANTEATER_CSVGRAPH_TEST_CASES(loop-detector loop-detector-1 loop-detection-1.xml)
ANTEATER_CSVGRAPH_TEST_CASES(loop-detector loop-detector-2 loop-detection-2.xml)
ANTEATER_CSVGRAPH_TEST_CASES(loop-detector loop-detector-3 loop-detection-3.xml)
ANTEATER_CSVGRAPH_TEST_CASES(loop-detector loop-detector-4 loop-detection-4.xml)

ANTEATER_CSVGRAPH_TEST_CASES(packet-loss packet-loss-1 packet-loss-1.xml)
ANTEATER_CSVGRAPH_TEST_CASES(packet-loss packet-loss-2 packet-loss-2.xml)
ANTEATER_CSVGRAPH_TEST_CASES(packet-loss packet-loss-3 packet-loss-3.xml)
#ANTEATER_CSVGRAPH_TEST_CASES(packet-loss packet-loss-4 packet-loss-4.xml)

ANTEATER_CSVGRAPH_TEST_CASES(consistency-checker consistent-forwarding-1 consistent-forwarding-1.xml)
ANTEATER_CSVGRAPH_TEST_CASES(consistency-checker consistent-forwarding-2 consistent-forwarding-2.xml)
