#
#$DEBUG=true

def assert
  raise "Assertion failed !" unless yield if $DEBUG
end    

module Anteater
  class RoutingPrefix
    attr_reader :ip, :width
    def self.parse(prefix_str)
      ip_str, mask_str = prefix_str.split("/")
      width = mask_str.to_i
      ip = ip_str.split('.').inject(0) {|total,value| (total << 8 ) + value.to_i}
      return new(ip, width)
    end

    def initialize(ip, width)
      @width = width
      @ip = ip
    end

    def to_s
      return [24, 16, 8, 0].collect {|b| (@ip >> b) & 255}.join('.') + "/" + @width.to_s
    end

    def contains(rp)
      return (@width <= rp.width) && ((@ip ^ rp.ip) & bit_mask()) == 0
    end

    def generate_ip_matching(builder, ip_addr)
      return builder.true if self == RoutingPrefix.default_routing_prefix

      bit_mask = builder.constant_bitvector(bit_mask(), 32);
      return builder.create_eq(builder.create_and(ip_addr, bit_mask), builder.constant_bitvector(@ip, 32))
    end

    def self.default_routing_prefix
      @@default_routing_prefix ||= RoutingPrefix.new(0, 0)
      return @@default_routing_prefix
    end

    def ==(rhs)
      return @ip == rhs.ip && @width == rhs.width
    end

    def bit_mask
      return (-1) << (32 - @width)
    end

  end

  class LongestPrefixMatchingTree
    INVALID_INTERFACE_ID = -1
    PAIR_IF_ID = 0
    PAIR_PREFIX = 1
    attr_reader :if_id, :destinations

    attr_accessor :range_begin, :range_end, :children, :prefix

    def self.create
        return new(INVALID_INTERFACE_ID, RoutingPrefix.default_routing_prefix)
    end

    def initialize(if_id, prefix)
      @if_id = Set.new.add(if_id)
      @prefix = prefix
      @children = []
      @destinations = []
    end
        
    def add_prefixes(list)
      @destinations.concat(list)
    end

    def generate_constraint(builder, ip_addr, if_id)
      # Generate the constraint directly instead of calling RoutingPrefix::generate_ip_matching, to avoid generating the instruction and %ip, mask multiple times.
      # The result .bc file is 1/4 smaller.
      # The GVN passes can achieve the same result, but it has a huge saving towards generation time
      mask_cache = Hash.new
      result = nil
      each_root_node_with_if_id(if_id) do |node|
        constraint = ip_matching(builder, node.prefix, mask_cache, ip_addr)

        sub_constraints = []
        each_direct_descendet_without_if_id(node, if_id) do |child|
          assert { !child.if_id.include?(if_id) }
          sub_constraints << ip_matching(builder, child.prefix, mask_cache, ip_addr)
        end

        sub_constraint = sub_constraints.reduce { |v, rhs| builder.create_or(v,rhs) }

        constraint = builder.create_and(constraint, builder.create_not(sub_constraint)) if sub_constraint
        
        result = result ? builder.create_or(result, constraint) : constraint
      end
      return result
    end

    def optimize!
      @range_begin = @prefix.ip
      @range_end = @prefix.ip + (1 << (32 - prefix.width))

      @children.each { |c| c.optimize! }
      return if @children.empty?
      
      @children.sort! {|x,y| x.range_begin <=> y.range_begin }

      # Phase 1: Merge children
      i = 0
      while i < @children.length - 1
        j = i + 1
        while j < @children.length and @children[i].if_id == @children[j].if_id and @children[j].range_begin <= @children[i].range_end
          @children[i].range_end = [@children[i].range_end, @children[j].range_end].max
          @children[i].children.concat(@children[j].children)
          j += 1
        end
        @children.slice!(i + 1, j - (i + 1))
        i += 1
      end

      # Phase 2: Normalization
      i = 0
      while i < @children.length
        c = @children[i]
        width = 0
        # MSB
        if c.range_begin == 0
          assert { c.range_end != 0 }
          width = 32 - LongestPrefixMatchingTree.msb(c.range_end)
        else
          width = 32 - LongestPrefixMatchingTree.lsb(c.range_begin)
          while width < 32 and (c.range_begin + (1 << (32 - width))) > c.range_end
            width += 1
          end
        end

        if c.range_begin + (1 << (32 - width)) < c.range_end
          c.prefix = Anteater::RoutingPrefix.new(c.range_begin, width)
          new_node = LongestPrefixMatchingTree.new(c.if_id, RoutingPrefix.default_routing_prefix)
          # HACK
          new_node.if_id.flatten!
          new_node.range_end = c.range_end
          c.range_end = c.range_begin + (1 << (32 - width))
          new_node.range_begin = c.range_end
          @children.insert(i + 1, new_node)
          j = c.children.index { |grand_child| !c.prefix.contains(grand_child.prefix) }
          new_node.children = c.children.slice!(j, c.children.length - j) if j

        else
          assert { c.range_begin + (1 << (32 - width)) == c.range_end }          
          c.prefix = Anteater::RoutingPrefix.new(c.range_begin, width)
        end

        i += 1
      end
    end

    protected(:children, :range_begin, :range_end, :prefix=, :optimize!)

    def each_root_node_with_if_id(id)
      q = [[nil, self]]
      while !q.empty?
        top = q.slice!(0)
        yield top[1] if top[1].if_id.include?(id) and !(top[0] and top[0].if_id.include?(id))
        q.concat(top[1].children.map {|c| [top[1],c]})
      end
    end

    def each_direct_descendet_without_if_id(node, id)
      q = [].concat(node.children)
      while !q.empty?
        top = q.slice!(0)
        if !top.if_id.include?(id)
          yield top
        else
          q.concat(top.children)
        end
      end
    end

    def self.lsb(i)
      return -1 if i == 0
      ret = 0
      while i & 1 == 0
        ret += 1
        i >>= 1
      end
      return ret
    end

    def self.msb(i)
      return -1 if i == 0
      ret = 0
      while i != 0
        ret += 1
        i >>= 1
      end
      return ret
    end

    def construct_tree
      @destinations.sort! { |x,y| x[PAIR_PREFIX].width <=> y[PAIR_PREFIX].width }
            
      while !@destinations.empty? and @destinations[0][PAIR_PREFIX] == RoutingPrefix.default_routing_prefix
        @if_id.delete(INVALID_INTERFACE_ID).add(@destinations.first[PAIR_IF_ID])
        @destinations.slice!(0)
      end

      @destinations.each do |dest|
        parent = find_best_match(dest[PAIR_PREFIX])
        if parent.prefix == dest[PAIR_PREFIX]
          parent.if_id.add(dest[PAIR_IF_ID])
        elsif !parent.if_id.include?(dest[PAIR_IF_ID])
          parent.children << LongestPrefixMatchingTree.new(dest[PAIR_IF_ID], dest[PAIR_PREFIX])
        end
      end
      optimize!
    end

    def dump
      puts 'digraph G {'
      q = [self]
      while !q.empty?
        c = q.slice!(0)
        puts '%d [label="%s|%s"];' % [c.object_id, c.prefix, c.if_id.inspect]
        q.concat(c.children)
      end

      q = [self]
      while !q.empty?
        c = q.slice!(0)
        c.children.each {|c1| puts '%d->%d;' % [c.object_id, c1.object_id] }
        q.concat(c.children)
      end
      puts '};'

    end

    private

    def find_best_match(rp)
      result = self
      while true do
        l = result.children.index { |x| x.prefix.contains(rp) }
        if l
          result = result.children[l]
        else
          break
        end
      end
      return result
    end

    def ip_matching(builder, rp, mask_cache, ip_addr)
      return builder.true if rp == RoutingPrefix.default_routing_prefix
      bit_mask = builder.constant_bitvector(rp.bit_mask, 32)
      mask_cache[rp.width] ||= builder.create_and(ip_addr, bit_mask)
      return builder.create_eq(builder.constant_bitvector(rp.ip, 32), mask_cache[rp.width])
    end
    
  end

  # Graph annotated with boolean constraints
  class ConstraintGraph
    attr_accessor :vertices
    def initialize
      @vertices = []
    end
    def size
      @vertices.length
    end
    def createVertex
      v = Vertex.new(size)
      @vertices << v
      v
    end

    class Vertex
      attr_accessor :edges
      attr_reader :idx
      def initialize(idx)
        @idx = idx
        @edges = []
      end

      def createEdge(target)
        e = Edge.new(target)
        @edges << e
        e
      end

      def clearAllEdges(g)
        g.vertices.each { |v| v.edges.reject! {|e| e.target == self } }       
        @edges.clear
      end

    end

    class Edge
      attr_accessor :target, :policy_function, :transformation_function
      def initialize(target)
        @target = target
      end
    end
  end

  # Dynamic programming
  def dp_reach(dst_vertex, max_hops, builder, g, edge_constraint)
    n = g.size
    d = Array.new(n) { Array.new(n) }
    g.vertices.each do |v|     
      v.edges.each do |e|
        d[v.idx][0] = edge_constraint.call(builder, e, 0) if e.target == dst_vertex
      end
    end
    
    for hops in 0..max_hops - 1
      g.vertices.each do |v|
        v.edges.each do |e|
          ir_v = d[e.target.idx][hops]
          if ir_v
            edge_v = edge_constraint.call(builder, e, hops + 1)
            p = builder.create_and(ir_v, edge_v)
            d[v.idx][hops + 1] = d[v.idx][hops + 1] ? builder.create_or(d[v.idx][hops + 1], p) : p
          end
        end
      end
    end
    return d
  end

  # edge callback for network without pkt transformation
  def reach_edge_constraint(pkt)
    return lambda do |builder, e, step|
      builder.create_call_edge_function(e.policy_function, pkt, pkt)
    end
  end

  def all_edge_constraint(sym_packets)
    return lambda do |builder, e, step|
      builder.create_and(builder.create_call_edge_function(e.policy_function, sym_packets[step + 1], sym_packets[step]),
                            builder.create_call_edge_function(e.transformation_function, sym_packets[step + 1], sym_packets[step]))
    end
  end

  module_function :dp_reach, :all_edge_constraint, :reach_edge_constraint
end

#$DEBUG = true
#t = Anteater::LongestPrefixMatchingTree::create()
#t.add_prefix(0, Anteater::RoutingPrefix.new("0.0.0.0/0"))
#t.generate_constraint(AnteaterCore::IRBuilder.new(), nil, 0)
