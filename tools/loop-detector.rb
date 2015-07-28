require ::File.expand_path(::File.dirname(__FILE__)) + "/../lib/Ruby/checkers.rb"

csv_graph = CSVGraph::Graph.create(ARGV[0])
csv_graph.optimize!
#csv_graph.dump

Anteater::SymbolicPacket::set_fields([32])
builder = Anteater::IRBuilder.create

node_mapping, g = csv_graph.to_network_graph(builder, {"dst_ip" => 0})

builder.dump("lc-base.bc")
builder.clear_constraint_functions

pkt = Anteater::SymbolicPacket.create
pkt.instantiate_to_ir(builder)

max_hops = g.size
if ARGV[1]
  max_hops = Integer(ARGV[1]) < max_hops ? Integer(ARGV[1]) : max_hops
end
	
checker = LoopDetector.new(builder, node_mapping, csv_graph, g)
checker.check(max_hops, Anteater::reach_edge_constraint(pkt))
