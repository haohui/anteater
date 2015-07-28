MACRO(ANTEATER_SLANG_TEST_CASES name file)
  add_test(${name} ${RUBY_EXECUTABLE} -I${PROJECT_BINARY_DIR}/lib/Ruby
    "${CMAKE_CURRENT_SOURCE_DIR}/slang-test.rb"
    "${CMAKE_CURRENT_SOURCE_DIR}/${file}"
    )
ENDMACRO(ANTEATER_SLANG_TEST_CASES)

ANTEATER_SLANG_TEST_CASES(security-breach security-breach.xml)
ANTEATER_SLANG_TEST_CASES(security-breach-2 security-breach-2.xml)
