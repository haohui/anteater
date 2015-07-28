require ::File.expand_path(::File.dirname(__FILE__)) + "/../../lib/Ruby/checkers.rb"
require ::File.expand_path(::File.dirname(__FILE__)) + "/../unit-test-lib.rb"

def get_pair_list(manifest)
  doc = REXML::Document.new(File.new(manifest))
  e = doc.get_elements('manifest/data/pair-list').first
  file_name = ::File.expand_path(::File.dirname(manifest) + ::File::SEPARATOR + e.attributes["file"])
  return ConsistentForwardingChecker.parse_list(file_name)
end

Anteater::SymbolicPacket::set_fields([32])
builder = Anteater::IRBuilder.create
csv_graph = CSVGraph::Graph.create(ARGV[0])
csv_graph.optimize!
#csv_graph.dump

node_mapping, g = csv_graph.to_network_graph(builder, {"dst_ip"=>0})

pkt = Anteater::SymbolicPacket.create
pkt.instantiate_to_ir(builder)

checker = ConsistentForwardingChecker.new(builder, node_mapping, csv_graph, g)
checker.check(get_pair_list(ARGV[0]), g.size(), Anteater::reach_edge_constraint(pkt))

ret = TestUtils::generate_result("cfc") == TestUtils::get_expected_result(ARGV[0])
TestUtils::clean_up("cfc") if !ENV["KEEP_FILES"]
exit(ret)
