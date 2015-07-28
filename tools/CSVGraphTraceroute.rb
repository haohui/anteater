require ::File.expand_path(::File.dirname(__FILE__) + "/../lib/Support/CSVGraph.rb")

manifest_file = ARGV[0]
start_vertex = ARGV[1]
prefix = ARGV[2]
MAX_HOPS = 30

def find_next_hop(current, prefix)
  result = []
  current.edges.each do |e|
    result.concat(e.prefixes.select {|p| p.contains(prefix) }.map {|p| [e.target,p]})
  end
  result.sort! {|x,y| y[1].width <=> x[1].width }   
  return result.empty? ? [nil, nil] : result.first
end

def output(trace)
  for i in 0..trace.length - 2
    puts "%s\t%s" % [trace[i][0].name, trace[i+1][1]]
  end
  puts trace.last[0].name
end

def search(trace, prefix)
  if trace.length > MAX_HOPS
    output(trace)
    return
  end

  next_hop, matched_prefix = find_next_hop(trace.last[0], prefix)

  if !next_hop
    output(trace)
    return
  else
    trace << [next_hop, matched_prefix]
    search(trace, prefix)
  end
end

csv_graph = CSVGraph::Graph.create(manifest_file)
#csv_graph.optimize!
v = csv_graph.vertices.select {|v| v.name == start_vertex}.first
p = Anteater::RoutingPrefix.parse(prefix)
search([[v, p]], p)

