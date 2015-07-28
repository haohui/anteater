require ::File.expand_path(::File.dirname(__FILE__)) + "/../lib/Ruby/checkers.rb"

Anteater::SymbolicPacket::set_fields([32])
builder = Anteater::IRBuilder.create

csv_graph = CSVGraph::Graph.create(ARGV[0])
csv_graph.optimize!
#csv_graph.dump

node_mapping, g = csv_graph.to_network_graph(builder, {"dst_ip"=>0})

builder.dump("cfc-base.bc")
builder.clear_constraint_functions

pkt = Anteater::SymbolicPacket.create
pkt.instantiate_to_ir(builder)

checker = ConsistentForwardingChecker.new(builder, node_mapping, csv_graph, g)
checker.check(ConsistentForwardingChecker.parse_list(ARGV[1]), g.size, Anteater::reach_edge_constraint(pkt))
