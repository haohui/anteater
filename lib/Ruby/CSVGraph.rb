require 'set'
require 'rexml/document'

require 'anteater_config'
require ::File.expand_path(::File.dirname(__FILE__)) + ::File::SEPARATOR + "anteater.rb"
require ::File.expand_path(::File.dirname(__FILE__)) + ::File::SEPARATOR + "UnionSet.rb"
require ::File.expand_path(::File.dirname(__FILE__)) + ::File::SEPARATOR + "parallel.rb"

require 'thread'

module CSVGraph
  class Graph
    include Anteater

    SPECIAL_VERTEX_LOOPBACK = 0
    SPECIAL_VERTEX_DROP = 1
    SPECIAL_VERTEX_NAT = 2
    SPECIAL_VERTEX_NAME = ["loopback", "drop", "nat"]
    
    # Fields for the FIB entries
    FIELD_DEST = 0
    FIELD_GATEWAY = 1
    FIELD_INTERFACE = 2
    FIELD_TAG = 3

    # Fields for the NAT rules
    NAT_FIELD_SRC_ROUTER = 0
    NAT_FIELD_SRC_PREFIX = 1
    NAT_FIELD_DST_PREFIX = 2

    attr_accessor :vertices, :hop_to_edge, :prefix_to_vertex
    def self.create(manifest)
      doc = REXML::Document.new(File.new(manifest))
      
      resolver_element = doc.get_elements('manifest/data/resolver').first
      resolver_file_name = nil
      if resolver_element
        resolver_file_name = ::File.expand_path(::File.dirname(manifest) + ::File::SEPARATOR + resolver_element.attributes["file"])
      end

      resolver = MergeResolver.new(resolver_file_name)

      to_be_parsed = []
      doc.elements.each('manifest/data/node') { |e| to_be_parsed << e }
      graphs = parallel_map(ANTEATER_CONCURRENT_JOBS, to_be_parsed) do |e|
        g = Graph.new
        self_vertex = Vertex.new(g, e.attributes['name'])
        self_vertex.flag |= Vertex::FLAG_RESOLVED
        g.parse(self_vertex, e.attributes['name'], ::File.expand_path(::File.dirname(manifest) + ::File::SEPARATOR + e.attributes["file"]))
        g.hop_to_edge = nil
        g.prefix_to_vertex = nil
        g
      end

      v = graphs.shift
      v.splice(resolver, graphs)
      graphs = nil
      GC.start

      # Parse NAT rules
      nat_element = doc.get_elements('manifest/data/nat').first
      v.parse_nat(::File.expand_path(::File.dirname(manifest) + ::File::SEPARATOR + nat_element.attributes["file"])) if nat_element
      
      return v
    end

    def initialize
      @vertices = []
      @special_vertices = Array.new(SPECIAL_VERTEX_NAME.length) do |i| 
        v = Vertex.new(self, SPECIAL_VERTEX_NAME[i])
        v.flag |= Vertex::FLAG_SPECIAL
        v
      end
      @hop_to_edge = Hash.new()
      @prefix_to_vertex = Hash.new()
    end

    def parse(self_vertex, name, file)
      File.open(file, "r") do |infile|
        while (line = infile.gets)
          next if line.start_with?("%")
          fields = line.strip().split(",")
          dest_prefix = RoutingPrefix.parse(fields[FIELD_DEST])
          next_hop = nil
          if fields[FIELD_GATEWAY] == "DIRECT"
            if fields[FIELD_INTERFACE].index("loopback") or fields[FIELD_INTERFACE] == "lb1"
              self_vertex.prefix = dest_prefix
              next_hop = @special_vertices[SPECIAL_VERTEX_LOOPBACK]

            elsif fields[FIELD_INTERFACE] == "drop"
              next_hop = @special_vertices[SPECIAL_VERTEX_DROP]

            else
              next_hop = get_vertex(dest_prefix)
            end

            next_hop.flag |= Vertex::FLAG_DESTINATION

          else
            next_hop = get_vertex(RoutingPrefix.new(CSVGraph::to_ip(fields[FIELD_GATEWAY]), 32))

          end
          
          assert { self_vertex != next_hop }
          
          edge = @hop_to_edge[dest_prefix]
          if !edge
            edge = Edge.new(self_vertex, next_hop)
            edge.name = fields[FIELD_INTERFACE]
            @hop_to_edge[dest_prefix] = edge
          end
          edge.prefixes << dest_prefix
          edge.tags << fields[FIELD_TAG]
        end
      end
    end

    def parse_nat(file)
      nat_v = @special_vertices[SPECIAL_VERTEX_NAT]
      drop_v = @special_vertices[SPECIAL_VERTEX_DROP]
      File.open(file, "r") do |infile|
        while (line = infile.gets)
          next if line.start_with?("%")

          fields = line.strip().split(",")
          src_v = @vertices[@vertices.index { |v| v.name == fields[NAT_FIELD_SRC_ROUTER] }]

          outbound_edge, inbound_edge, drop_edge = Edge.new(nat_v, src_v), Edge.new(src_v, nat_v), Edge.new(nat_v, drop_v)

          outbound_edge.prefixes << RoutingPrefix.parse(fields[NAT_FIELD_DST_PREFIX])
          outbound_edge.flag |= Edge::FLAG_NAT_OUTBOUND

          inbound_edge.prefixes << RoutingPrefix.default_routing_prefix
          inbound_edge.flag |= Edge::FLAG_NAT_INBOUND

          drop_edge.prefixes << RoutingPrefix.parse(fields[NAT_FIELD_SRC_PREFIX])

          [outbound_edge, inbound_edge].each do |e|
            e.nat_rules << [RoutingPrefix.parse(fields[NAT_FIELD_SRC_PREFIX]), RoutingPrefix.parse(fields[NAT_FIELD_DST_PREFIX])]
          end
        end      
      end
    end

    def splice(resolver, graphs)
      signatures = Hash.new()
      @vertices.each { |v| signatures[v.signature(resolver.equivalent_host(v.prefix, v.name))] = v }

      graphs.each do |g|
        node_map = Hash.new()
        g.vertices.each do |v|
          s = v.signature(resolver.equivalent_host(v.prefix, v.name))
          if signatures[s]
            node_map[v] = signatures[s]
            signatures[s].flag |= v.flag
            signatures[s].name = v.name if v.name
          else
            new_v = Vertex.new(self, v.name)
            new_v.flag, new_v.prefix = v.flag, v.prefix
            signatures[s] = new_v
            node_map[v] = new_v
          end
        end

        # Copy edges
        g.vertices.each do |v|
          v.edges.each { |e| e.target = node_map[e.target] }
          node_map[v].edges.concat(v.edges)
        end
      end
    end

    def optimize!
      # Merge all nodes whose out degree is zero, as well as multiple edges into one node
      redundant_vertices = @vertices.select { |v| v.edges.length == 0  and v.name != "drop"}
      return if redundant_vertices.length <= 1

      target = redundant_vertices.shift
      target.name = "merged"
      vertices_sets = redundant_vertices.to_set

      @vertices.each do |v|
        v.edges.each do |e|
          e.target = target if vertices_sets.include?(e.target)
        end
      end

      @vertices.reject! {|v| vertices_sets.include?(v) }

      # Merge all edges
      @vertices.each do |v|
        targets = Hash.new
        v.edges.each do |e|
          if targets[e.target]
            targets[e.target] << e
          else
            targets[e.target] = [e]
          end
        end

        edges_to_be_deleted = Set.new
        targets.each_pair do |_,edges|
          next if edges.length == 1
          saved_edge = edges.shift
          edges.each { |e| saved_edge.splice(e) }
          edges_to_be_deleted.merge(edges)
          
        end
        v.edges.reject! {|e| edges_to_be_deleted.include?(e) }
      end
    end

    def to_network_graph(builder, idx_map)
      dst_ip_idx = idx_map["dst_ip"]
      g = Anteater::ConstraintGraph.new
      node_mapping = Hash.new
      prefix_tree_mapping = Hash.new
      trees = []
      @vertices.each do |v|
        node_mapping[v] = g.createVertex
        tree = Anteater::LongestPrefixMatchingTree.create
        trees << tree
        prefix_tree_mapping[v] = tree
        v.edges.each do |e|
          tree.add_prefixes(e.prefixes.map { |p| [e.object_id, p] })
        end
      end
      
      parallel_each(ANTEATER_CONCURRENT_JOBS, trees) { |tree| tree.construct_tree }
      trees = nil

      @vertices.each do |v|
        v.edges.each do |e|
          edge = node_mapping[v].createEdge(node_mapping[e.target])

          p_in, p_out, policy_function = builder.create_edge_function
          ip_addr = builder.create_load(p_out.get(dst_ip_idx, builder))
          fib_constraint = prefix_tree_mapping[v].generate_constraint(builder, ip_addr, e.object_id)
          
          builder.create_ret(fib_constraint)

          edge.policy_function = policy_function
          if e.nat_rules.empty?
            edge.transformation_function = builder.identical_transformation
          else
            # Notice that p_in / p_out has changed their values..
            p_in, p_out, edge.transformation_function = builder.create_edge_function
            e.generate_nat_constraint(builder, p_in, p_out, dst_ip_idx)
          end
        end
      end
      return [node_mapping, g]
    end

    def dump
      puts 'digraph G {'
      vertex_idx_map = Hash.new()
      @vertices.each_index {|i| vertex_idx_map[@vertices[i]] = i}
      @vertices.each do |v|
        puts '%d [label="%s|%d"];' % [vertex_idx_map[v], v.name ? v.name : v.prefix, v.flag]
      end
      @vertices.each do |v|
        v.edges.each do |e|
          puts '%d->%d;' % [vertex_idx_map[v], vertex_idx_map[e.target]]
        end
      end
      puts '};'
    end
    
    private
    def get_vertex(prefix)
      v = @prefix_to_vertex[prefix]
      if !v
        v = Vertex.new(self, nil)
        v.prefix = prefix
        @prefix_to_vertex[prefix] = v
      end
      
      return v
    end    

  end

  class Vertex
    FLAG_RESOLVED = 1
    FLAG_DESTINATION = 2
    FLAG_SPECIAL = 4
    attr_accessor :name, :flag, :prefix, :edges
    def initialize(graph, name)
      @name = name
      @flag = 0
      @edges = []
      graph.vertices << self
    end

    # Signature <SPECIAL(1)><SPECIAL IDX(2)><WIDTH(6)><IP(32)>
    def signature(prefix)
      res = 0

      if @flag & FLAG_SPECIAL == FLAG_SPECIAL
        res = (1 << (2 + 6 + 32)) | (Graph::SPECIAL_VERTEX_NAME.index {|v| v == @name} << (6 + 32))
      end
      if prefix
        res |= (prefix.width << 32) | prefix.ip
      elsif @flag & FLAG_RESOLVED == FLAG_RESOLVED and @name
        res |= @name.hash & 0xffffffff
      end
      return res
    end
  end

  class Edge
    FLAG_NAT_INBOUND = 1
    FLAG_NAT_OUTBOUND = 2
    attr_accessor :tags, :target, :name, :prefixes, :nat_rules, :flag
    def initialize(src, dest)
      @flag = 0
      @tags = Set.new
      @target = dest
      @prefixes = []
      @nat_rules = []
      src.edges << self
    end

    def splice(e)
      @prefixes.concat(e.prefixes)
      @nat_rules.concat(e.nat_rules)
    end

    def generate_nat_constraint(builder, p_in, p_out, dst_ip_idx)
      assert { (@flag & FLAG_NAT_INBOUND) ^ (@flag & FLAG_NAT_OUTBOUND) == 1 }
      in_ip_addr, out_ip_addr = [p_in, p_out].map {|p| builder.create_load(p.get(dst_ip_idx, builder)) }
      
      o = @flag & FLAG_NAT_INBOUND == FLAG_NAT_INBOUND ? [1,0] : [0,1]
      
      c_nat = @nat_rules.map {|r| builder.create_and(r[o[0]].generate_ip_matching(builder, in_ip_addr), 
                                                        r[o[1]].generate_ip_matching(builder, out_ip_addr)) }.reduce do |v, rhs|
        builder.create_or(v, rhs)
      end
      
      builder.create_ret(c_nat)
    end
  end

  class MergeResolver
    def initialize(filename)
      @ip_to_node = Hash.new()
      @node_to_ip = Hash.new()
      @name_to_node = Hash.new()
      parse(filename)
    end

    def parse(filename)
      return if !filename
      File.open(filename, "r") do |infile|
        while (line = infile.gets)
          next if line.start_with?("%")

          fields = line.strip().split(",")
          name = fields.shift
          nodes = []
          fields.each do |f|
            ip = CSVGraph::to_ip(f)
            $stderr.puts "[MergeReoslver] Multiple IP %s" % f if @ip_to_node[ip]
            node = UnionSet.new()
            nodes << node
            @ip_to_node[ip] = node
            @node_to_ip[node] = ip
          end

          v = nodes.shift
          nodes.each { |u| v.union(u) }
          @name_to_node[name] = v

        end
      end
    end

    def equivalent_host(prefix, name)
      if !prefix && @name_to_node[name]
        return Anteater::RoutingPrefix.new(@node_to_ip[@name_to_node[name].find], 32)
      end

      return prefix if !prefix or prefix.width != 32 or !@ip_to_node[prefix.ip]
      return Anteater::RoutingPrefix.new(@node_to_ip[@ip_to_node[prefix.ip].find], 32)
    end

  end

  def to_ip(ip_str)
    @ip = ip_str.split('.').inject(0) {|total,value| (total << 8 ) + value.to_i}
  end
  module_function :to_ip
end

