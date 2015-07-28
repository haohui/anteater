require ::File.expand_path(::File.dirname(__FILE__)) + "/../unit-test-lib.rb"

def main
  check_all_pair_reachability(ARGV[0])
  ret = TestUtils::generate_result("aprc") == TestUtils::get_expected_result(ARGV[0])
  TestUtils::clean_up("aprc") if !ENV["KEEP_FILES"]
  return ret
end

exit(main())

