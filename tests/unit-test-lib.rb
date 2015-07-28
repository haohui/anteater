require 'rexml/document'
require ::File.expand_path(::File.dirname(__FILE__)) + "/../lib/Ruby/anteater.rb"
require ::File.expand_path(::File.dirname(__FILE__)) + "/../lib/Ruby/anteater-ffi.rb"


PKT_DST_IP = 0
PKT_SRC_IP = 1
PKT_DST_PORT = 2
PKT_SRC_PORT = 3

FIELD_MAPPING = Hash["dest" => PKT_DST_IP, "src" => PKT_SRC_IP, "src-port" => PKT_SRC_PORT, "dst-port" => PKT_DST_PORT]

class Router
  attr_accessor :id, :name, :interfaces, :fib_entries
  def initialize(id, name)
    @id = id
    @name = name
    @interfaces = []
    @fib_entries = Anteater::LongestPrefixMatchingTree.create()
  end
end

class Interface
  attr_accessor :id, :nexthop_id, :acl_rules, :nat_rules, :field_rules
  def initialize(id, nexthop_id)
    @id = Integer(id)
    @nexthop_id = nexthop_id
    @acl_rules = []
    @nat_rules = []
    @field_rules = []
  end

  def generate_acl_constraints(builder, pkt)
    return builder.true if @acl_rules.length == 0

    res = builder.false
    pre_condition = builder.true
    acl_rules.each do |r|
      c = r.generate_constraint(builder,pkt)
      res = builder.create_or(res, builder.create_and(pre_condition, c)) if r.action == ACLRule::ACTION_ACCEPT
      pre_condition = builder.create_and(pre_condition, builder.create_not(c))
    end
    res = builder.create_or(res, pre_condition)
    return res
  end

  def generate_nat_constraints(builder, p_in, p_out)
    constraints = @nat_rules.map do |rule|
      builder.create_and(rule[0].generate_ip_matching(builder, builder.create_load(p_in.get(PKT_DST_IP, builder))),
                            rule[1].generate_ip_matching(builder, builder.create_load(p_out.get(PKT_DST_IP, builder))))
    end
    return constraints.reduce { |v, rhs| builder.create_or(v, rhs) }
  end

  def generate_field_constraints(builder, p_in, p_out)
    constraints = @field_rules.map do |rule|
      rule.generate_constraint(builder, p_in, p_out)
    end
    return constraints.reduce { |v, rhs| builder.create_and(v, rhs) }
  end

  def has_transformation
    return @nat_rules.length > 0 || @field_rules.length > 0
  end
end

class ACLRule
  ACTION_ACCEPT = 0
  ACTION_DENY = 1
  attr_reader :action

  def initialize(e)
    @action = e.attributes["action"] == "accept" ? ACTION_ACCEPT : ACTION_DENY
    @e = e
  end

  def generate_constraint(builder, pkt)
    constraints = []
    @e.elements.each('field') do |field|
      idx = field.attributes["name"] ? FIELD_MAPPING[field.attributes["name"]] : Integer(field.attributes["id"])
      case field.attributes["op"]
      when "eq"
        v = field.elements.first
        case v.name
        when "prefix"
          constraints << Anteater::RoutingPrefix.parse(v.text()).generate_ip_matching(builder, builder.create_load(pkt.get(idx, builder)))

        when "integer"
          constraints << builder.create_eq(builder.create_load(pkt.get(idx, builder)), builder.constant_int(Integer(v.attributes["value"]), Integer(v.attributes["length"])))
        end
      end
    end

    return constraints.reduce(builder.true) do |v, rhs|
      builder.create_and(v, rhs)
    end
  end

end

class FieldRule
  def initialize(e)
    @e = e
  end

  def generate_constraint(builder, p_in, p_out)
    constraints = []
    @e.elements.each do |field|
      idx = field.attributes["name"] ? FIELD_MAPPING[field.attributes["name"]] : Integer(field.attributes["id"])

      if field.attributes["op"] == "identical"

        constraints << builder.create_eq(builder.create_load(p_in.get(idx, builder)), 
                                                      builder.create_load(p_out.get(idx, builder)))

      else
        v = field.elements.first
        case v.name
        when "prefix"
          constraints << Anteater::RoutingPrefix.new(v.text()).generate_ip_matching(builder, builder.create_load(p_out.get(idx, builder)))

        when "integer"
          constraints << builder.create_eq(builder.create_load(p_out.get(idx, builder)), builder.constant_int(Integer(v.attributes["value"]), Integer(v.attributes["length"])))
        end
      end
    end
    return constraints.reduce(builder.true) {| v, rhs| builder.create_and(v, rhs) }
  end
end


def parse_data(data)
  all_routers = []
  doc = REXML::Document.new(data)
  doc.elements.each('test/network/router') do |r|
    router = Router.new(r.attributes["id"], r.attributes["name"])
    all_routers[Integer(r.attributes["id"])] = router
    r.elements.each('interface') do |interface|
      net_if = Interface.new(interface.attributes["id"], interface.attributes["nexthop_id"])
      router.interfaces << net_if
      interface.elements.each do |e|
        case e.name
        when "fib"
          router.fib_entries.add_prefixes([[net_if.id, Anteater::RoutingPrefix.parse(e.attributes["dest"])]])

        when "acl"
          net_if.acl_rules << ACLRule.new(e)

        when "transformation-nat"
          net_if.nat_rules << [Anteater::RoutingPrefix.parse(e.attributes["src"]), Anteater::RoutingPrefix.parse(e.attributes["dest"])]

        when "transformation-setfield"
          net_if.field_rules << FieldRule.new(e)

        end
      end
    end
  end
  return all_routers
end

def construct_graph(builder, routers)
  g = Anteater::ConstraintGraph.new
  node_mapping = Hash.new
  routers.each { |r| node_mapping[r] = g.createVertex; }
  routers.each do |r|
    r.fib_entries.construct_tree

    r.interfaces.each do |net_if|
      edge = node_mapping[r].createEdge(node_mapping[routers[Integer(net_if.nexthop_id)]])
      p_in, p_out, policy_function = builder.create_edge_function
      ip_addr = builder.create_load(p_out.get(PKT_DST_IP, builder))
      fib_constraint = r.fib_entries.generate_constraint(builder, ip_addr, net_if.id)
      acl_constraint = net_if.generate_acl_constraints(builder, p_out)
      builder.create_ret(builder.create_and(fib_constraint, acl_constraint))
      edge.policy_function = policy_function

      if net_if.has_transformation
        p_in, p_out, transformation_function = builder.create_edge_function
        constraint = [net_if.generate_nat_constraints(builder, p_in, p_out), 
                      net_if.generate_field_constraints(builder, p_in, p_out)].compact().reduce() { |v, rhs| builder.create_and(v, rhs)}
        builder.create_ret(constraint)
        edge.transformation_function = transformation_function

      else
        edge.transformation_function = builder.identical_transformation

      end
    end
  end
  return [node_mapping, g]
end
  

def check_for_destination(dst_vertex, sym_packets, builder, g, counter)
  n = g.size()
  d = Anteater::dp_reach(dst_vertex, n - 1, builder, g, Anteater::all_edge_constraint(sym_packets))
  is_empty = d.index {|d1| d1.compact.length > 0 }

  g.vertices.each do |v|
    next if v == dst_vertex or is_empty == nil
    builder.checkpoint_main_function
    ir_v = d[v.idx].compact.reduce { |v, rhs| builder.create_or(v, rhs) }
    next if !ir_v
    builder.assert(ir_v)
    builder.dump("aprc-%d.bc" % counter)
    counter += 1
    builder.rewind_to_checkpoint
  end
  return counter
end

def calculate_reachability(builder, g)
  counter = 0
  builder.create_main_function
  n = g.size
  sym_packets = Array.new(n + 1) { p = Anteater::SymbolicPacket.create; p.instantiate_to_ir(builder); p }

  g.vertices.each do |v|
    builder.clear_main_function
    counter = check_for_destination(v, sym_packets, builder, g, counter)
  end
end

def check_all_pair_reachability(filename)
  Anteater::SymbolicPacket::set_fields([32,32,16,16,1])
  builder = Anteater::IRBuilder.create
  g = construct_graph(builder, parse_data(File.new(filename)))[1]
  calculate_reachability(builder, g)
end

module TestUtils
  def self.generate_result(prefix)
    makefile = ::File.expand_path(::File.dirname(__FILE__) + "/../tools/scripts") + ::File::SEPARATOR + "Makefile.solve"

    system("make -f %s -r all_results -r TIMED=0 BC_SRCS=\"%s\"" % [makefile, Dir.glob("%s-*.bc" % prefix).join(" ")])
    sat_instances = 0
    unsat_instances = 0
    Dir.glob("%s-*.result" % prefix).each do |file| 
      File.new(file).readlines().map do |l|
        l.strip!()
        if l == "sat"
          sat_instances += 1
        elsif l == "unsat"
          unsat_instances += 1
        end
      end
    end
    return [sat_instances, unsat_instances]
  end
 
  def self.clean_up(prefix)
    Dir.glob("%s-*.bc" % prefix).each { |f| File.delete(f) }
    Dir.glob("%s-*.result" % prefix).each { |f| File.delete(f) }
  end
  
  def self.get_expected_result(filename)
    doc = REXML::Document.new(File.new(filename))
    doc.elements.each('//test/result') do |e|
      return [Integer(e.attributes["sat"]), Integer(e.attributes["unsat"])]
    end
  end

end
