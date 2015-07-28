require ::File.expand_path(::File.dirname(__FILE__)) + "/../../contrib/ruby-prolog/ruby-prolog.rb"
require ::File.expand_path(::File.dirname(__FILE__)) + "/../unit-test-lib.rb"

TAG_INDEX = 1

module RubyUtils
  def add_tag_rule(fw, internal_router)
    # Add transformation rule to take the packet going into internal network
    fw.interfaces.select {|net_if| net_if.nexthop_id == internal_router.id }.each do |net_if|
      rule = '<transformation-setfield><field id="%d"><integer value="1" length="1"/></field></transformation-setfield>' % TAG_INDEX
      net_if.field_rules << FieldRule.new(REXML::Document.new(rule).root())
    
      for i in 0..TAG_INDEX - 1
        identical_rule = '<transformation-setfield><field id="%d" op="identical"/></transformation-setfield>' % i
        net_if.field_rules << FieldRule.new(REXML::Document.new(identical_rule).root())
      end
    end
  end
  module_function :add_tag_rule
end

Anteater::SymbolicPacket::set_fields([32,1])
builder = Anteater::IRBuilder.create
builder.create_main_function

all_routers = parse_data(File.new(ARGV[0]))

c = RubyProlog::Core.new
c.instance_eval do
 
  all_routers.select {|r| r.name == "fw"}.each { |r| router[r, "firewall"].fact }
  all_routers.select {|r| r.name == "ext-router"}.each { |r| router[r, "ext-router"].fact }

  all_routers.select {|r| r.name.start_with?("192.17") }.each do |r|
    router[r, "internal"].fact
  end

  add_tag_rule[:FW_ROUTER,:INTERNAL_ROUTER].calls do |env|
    RubyUtils::add_tag_rule(env[:FW_ROUTER], env[:INTERNAL_ROUTER])
    true
  end

  build_graph[:GRAPH,:MAPPING,:SYM_PACKETS].calls do |env|
    node_mapping, g = construct_graph(builder, all_routers)
    n = g.size
    sym_packets = Array.new(n + 1) { p = Anteater::SymbolicPacket.create; p.instantiate_to_ir(builder); p }

    env.unify(env[:GRAPH], g)
    env.unify(env[:MAPPING], node_mapping)
    env.unify(env[:SYM_PACKETS], sym_packets)
  end

  reset_file[].calls { builder.clear_main_function; true }
  write_file[:PREFIX].calls do |env|
    @counter ||= 0
    builder.dump("%s-%d.bc" % [env[:PREFIX], @counter])
    @counter += 1
    true
  end

  reach[:RESULT,:DEST,:MAX_HOPS,:SYM_PACKETS,:GRAPH].calls do |env|
    d = Anteater::dp_reach(env[:DEST], env[:MAX_HOPS], builder, env[:GRAPH], Anteater.all_edge_constraint(env[:SYM_PACKETS]))
    env.unify(env[:RESULT], d)
  end

  collect_constraint[:RES,:D,:SRC].calls do |env|
    ir_v = env[:D][env[:SRC].idx].compact.reduce { |v, rhs| builder.create_or(v, rhs) }
    env.unify(env[:RES], ir_v)
  end

  ir_integer_value[:RES,:VALUE,:LENGTH].calls do |env|
    res = builder.constant_int(env[:VALUE], env[:LENGTH])
    env.unify(env[:RES], res)
  end

  field_eq[:RES,:PACKET,:INDEX,:VALUE].calls do |env|
    res = builder.create_eq(builder.create_load(env[:PACKET].get(env[:INDEX], builder)), env[:VALUE])
    env.unify(env[:RES], res)   
  end

  and_expr[:RES,:LHS,:RHS].calls do |env|
    res = builder.create_and(env[:LHS], env[:RHS])
    env.unify(env[:RES], res)
  end

  assertion[:VALUE].calls { |env| builder.assert(env[:VALUE]); true }

  graph_size[:N,:G].calls { |env| env.unify(env[:N], env[:G].size()) }
  substract[:R,:LHS,:RHS].calls { |env| env.unify(env[:R], env[:LHS] - env[:RHS]) }
  
  deref[:RES,:IDX,:ARRAY].calls { |env| env.unify(env[:RES], env[:ARRAY][env[:IDX]]) }

  tag_rules[] <<= [
    router[:fw_router, "firewall"], 
    router[:internal_router, "internal"],
    add_tag_rule[:fw_router, :internal_router],
  ]  

  generate_query[:EXT_ROUTER,:INTERNAL_ROUTER,:GRAPH,:SYM_PACKETS] <<= [
    reset_file[],
    graph_size[:n, :GRAPH],
    substract[:hops, :n, 1],
    reach[:d,:INTERNAL_ROUTER,:hops,:SYM_PACKETS,:GRAPH],
    collect_constraint[:constraint,:d,:EXT_ROUTER],
    deref[:pkt,0,:SYM_PACKETS],
    ir_integer_value[:v, 0, 1],
    field_eq[:additional_constraint,:pkt,TAG_INDEX,:v],
    and_expr[:final,:constraint,:additional_constraint],
    assertion[:final],
    write_file["slang"]
]   
                
  goal[] <<= [
    build_graph[:g, :mapping, :sym_packets],
    :CUT,
    router[:ext_router, "ext-router"], 
    router[:internal_router, "internal"],
    deref[:cg_ext_router,:ext_router,:mapping],
    deref[:cg_internal_router,:ext_router,:mapping],
    generate_query[:cg_ext_router, :cg_internal_router, :g, :sym_packets]
  ]

  query(tag_rules[])
  query(goal[])
end

ret = TestUtils::generate_result("slang") == TestUtils::get_expected_result(ARGV[0])
TestUtils::clean_up("slang") if !ENV["KEEP_FILES"]
