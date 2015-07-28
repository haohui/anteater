MACRO(ANTEATER_APRC_TEST_CASES name file)
  add_test(${name} ${RUBY_EXECUTABLE} -I${PROJECT_BINARY_DIR}/lib/Ruby
    "${CMAKE_CURRENT_SOURCE_DIR}/all-pair-reachability-checker.rb"
    "${CMAKE_CURRENT_SOURCE_DIR}/${file}"
    )
ENDMACRO(ANTEATER_APRC_TEST_CASES)

ANTEATER_APRC_TEST_CASES(longest-prefix-matching-1 longest-prefix-matching-1.xml)
ANTEATER_APRC_TEST_CASES(longest-prefix-matching-2 longest-prefix-matching-2.xml)
ANTEATER_APRC_TEST_CASES(longest-prefix-matching-3 longest-prefix-matching-3.xml)
ANTEATER_APRC_TEST_CASES(bridge bridge.xml)
ANTEATER_APRC_TEST_CASES(default-routing default-routing.xml)
ANTEATER_APRC_TEST_CASES(single-acl-1 single-acl-1.xml)
ANTEATER_APRC_TEST_CASES(single-acl-2 single-acl-2.xml)
ANTEATER_APRC_TEST_CASES(orthogonal-acl orthogonal-acl.xml)
ANTEATER_APRC_TEST_CASES(nat nat.xml)
ANTEATER_APRC_TEST_CASES(nat-2 nat-2.xml)
