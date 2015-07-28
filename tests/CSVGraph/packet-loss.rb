require ::File.expand_path(::File.dirname(__FILE__)) + "/../../lib/Ruby/checkers.rb"
require ::File.expand_path(::File.dirname(__FILE__)) + "/../unit-test-lib.rb"

Anteater::SymbolicPacket::set_fields([32])
builder = Anteater::IRBuilder.create

csv_graph = CSVGraph::Graph.create(ARGV[0])
csv_graph.optimize!
#csv_graph.dump
node_mapping, g = csv_graph.to_network_graph(builder, {"dst_ip"=>0})

pkt = Anteater::SymbolicPacket.create
pkt.instantiate_to_ir(builder)

checker = PacketLossDetector.new(builder, node_mapping, csv_graph, g)
checker.check(g.size, Anteater::reach_edge_constraint(pkt))

ret = TestUtils::generate_result("pl") == TestUtils::get_expected_result(ARGV[0])
TestUtils::clean_up("pl") if !ENV["KEEP_FILES"]
exit(ret)
