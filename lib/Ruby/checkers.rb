require ::File.expand_path(::File.dirname(__FILE__)) + ::File::SEPARATOR + "CSVGraph.rb"
require ::File.expand_path(::File.dirname(__FILE__)) + ::File::SEPARATOR + "anteater-ffi.rb"

class LoopDetectorWithTransformation
  def initialize(builder, mapping, csv_graph, ng)
    @builder = builder
    @csv_graph = csv_graph
    @ng = ng
    @mapping = mapping
  end
  
  def check(max_hops)
    sym_packets = Array.new(max_hops + 2) { p = Anteater::SymbolicPacket.create; p.instantiate_to_ir(@builder); p }

    shadow_vertex = @ng.createVertex

    @builder.create_main_function
    @csv_graph.vertices.each do |v|
      next if v.flag & CSVGraph::Vertex::FLAG_RESOLVED == 0

      network_vertex = @mapping[v]
      copy_in_edge(network_vertex, shadow_vertex)

      @builder.clear_main_function
      d = Anteater::dp_reach(shadow_vertex, max_hops, @builder, @ng, Anteater::all_edge_constraint(sym_packets))
      ir_v = d[network_vertex.idx].compact().reduce() { |v, rhs| @builder.create_or(v, rhs) }
      next if ir_v == nil
      @builder.assert(ir_v)

#      d.each_index do |i|
#        d[i].each_index do |j|
#          @builder.recordTransformationConstraint(j, sym_packets[j + 1], sym_packets[j], d[i][j]) if d[i][j]
#        end
#      end

      @builder.dump("lc-%s.bc" % v.name)
      shadow_vertex.clearAllEdges(@ng)
    end
  end

  private
  def copy_in_edge(src, dst)
    @ng.vertices.each do |v|
      next if v == dst
      v.edges.each do |e|
        if e.target == src
          shadow_edge = v.createEdge(dst)
          shadow_edge.policy_function = e.policy_function
          shadow_edge.transformation_function = e.transformation_function
        end
      end
    end
  end
end

class LoopDetector
  def initialize(builder, mapping, csv_graph, ng)
    @builder = builder
    @csv_graph = csv_graph
    @ng = ng
    @mapping = mapping
  end
  
  def check(max_hops, edge_constraint_callback)
    shadow_vertex = @ng.createVertex

    @builder.create_main_function
    @csv_graph.vertices.each do |v|
      next if v.flag & CSVGraph::Vertex::FLAG_RESOLVED == 0

      network_vertex = @mapping[v]
      copy_in_edge(network_vertex, shadow_vertex)

      @builder.clear_main_function
      d = Anteater::dp_reach(shadow_vertex, max_hops, @builder, @ng, edge_constraint_callback)

      ir_v = d[network_vertex.idx].compact().reduce() { |v, rhs| @builder.create_or(v, rhs) }
      next if !ir_v

      filter_constraint = @builder.create_not(@builder.create_call_filter_function)
      @builder.assert(@builder.create_and(filter_constraint, ir_v))
      @builder.dump("lc-%s.bc" % v.name)
      shadow_vertex.clearAllEdges(@ng)
    end
  end

  private
  def copy_in_edge(src, dst)
    @ng.vertices.each do |v|
      next if v == dst
      v.edges.each do |e|
        if e.target == src
          shadow_edge = v.createEdge(dst)
          shadow_edge.policy_function = e.policy_function
          shadow_edge.transformation_function = e.transformation_function
        end
      end
    end
  end
end

class PacketLossDetector
  def initialize(builder, mapping, csv_graph, ng)
    @builder = builder
    @csv_graph = csv_graph
    @ng = ng
    @mapping = mapping
  end
  
  def check(max_hops, edge_constraint_callback)
    dst = @csv_graph.vertices.select { |v| v.name == "merged" }.first
    @builder.create_main_function
    d = Anteater::dp_reach(@mapping[dst], max_hops, @builder, @ng, edge_constraint_callback)
    @builder.checkpoint_main_function

    @csv_graph.vertices.each do |v|
      next if v.flag & CSVGraph::Vertex::FLAG_RESOLVED == 0

      network_vertex = @mapping[v]
      
      ir_v = d[network_vertex.idx].compact().reduce() { |v, rhs| @builder.create_or(v, rhs) }
      next if !ir_v
      
      filter_constraint = @builder.create_not(@builder.create_call_filter_function)
      @builder.assert(@builder.create_and(filter_constraint, @builder.create_not(ir_v)))

      @builder.dump("pl-%s.bc" % v.name)
      @builder.rewind_to_checkpoint
    end
  end
end

class ConsistentForwardingChecker
  def initialize(builder, mapping, csv_graph, ng)
    @builder = builder
    @csv_graph = csv_graph
    @ng = ng
    @mapping = mapping
  end
  
  def check(list, max_hops, edge_constraint_callback)
    # The checker only checks the consistency for the "merged" node
    vertices_name_map = Hash.new()
    @csv_graph.vertices.each {|v| vertices_name_map[v.name] = v if v.name }
    
    dst = vertices_name_map["merged"]
    @builder.create_main_function
    d = Anteater::dp_reach(@mapping[dst], max_hops, @builder, @ng, edge_constraint_callback)
    @builder.checkpoint_main_function

    list.each do |name1, name2|
      next if !vertices_name_map[name1] or !vertices_name_map[name2]

      values = [name1, name2].map do |n|
        d[@mapping[vertices_name_map[n]].idx].compact().reduce() { |v, rhs| @builder.create_or(v, rhs) }
      end
      
      values.each_index { |idx| values = @builder.false if !values[idx]}

      ir_v = @builder.create_xor(values[0], values[1])

      filter_constraint = @builder.create_not(@builder.create_call_filter_function)
      @builder.assert(@builder.create_and(filter_constraint, ir_v))

      @builder.dump("cfc-%s-%s.bc" % [name1, name2])
      @builder.rewind_to_checkpoint
    end
  end

  def self.parse_list(filename)
    res = []
    File.open(filename, "r") do |infile|
      while (line = infile.gets)
        next if line.start_with?("%")
        fields = line.strip().split(",")
        res << [fields[0], fields[1]]
      end
    end
    return res
  end
end
