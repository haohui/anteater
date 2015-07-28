require 'anteater_config'
require 'fileutils'
require 'tempfile'

def test_pkt_bc(base_file, pkt_bc)
  Kernel.system("#{LLVM_BIN_DIR}/llvm-ld", "-disable-opt", "-disable-verify", "-link-as-library", "-load", "#{ANTEATER_BUILD_DIR}/lib/Transform/libSPATransform.so", "-bare-gvn", "-globaldce", "-internalize", "-basiccg", "-inline", "-adce", "-gvn", "-instcombine", "-packet-scalar-repl", "-globaldce", base_file, pkt_bc, "-o", "#{pkt_bc}.linked")
  
  yices_result = nil
  IO.popen(["#{ANTEATER_BUILD_DIR}/tools/anteatercc/anteatercc", "-individual-assertion", "#{pkt_bc}.linked"], "r") do |p|
    IO.popen(["#{YICES_BIN}", "--timeout=30"], "r+") do |p1|
      p1.write p.read
      p1.close_write
      yices_result = p1.readlines
    end
  end
  sat_count, unknown_count = ["sat\n", "unknown\n"].map { |stat| yices_result.count(stat) }
  if sat_count == 1 and unknown_count == 0
    return File.basename(pkt_bc, ".bc").split("-")
  else
    return nil
  end
end

def generate_test_cases(bc_file)
  Kernel.system("#{ANTEATER_BUILD_DIR}/tools/packet-propagation-hint-generator/ppa-hint-generator", File::expand_path(bc_file))
end

def merge_packet(bc_file, list, output_file)
  pkt_hint_file = Tempfile.new("pkt-hint", Dir.pwd)
  list.each { |p| pkt_hint_file.puts p.join(",") }
  pkt_hint_file.close
  Kernel.system("#{LLVM_BIN_DIR}/opt", "-load", "#{ANTEATER_BUILD_DIR}/lib/Transform/libSPATransform.so", "-merge-packet", "-merge-packet-hint", pkt_hint_file.path, "-bare-gvn", "-globaldce", "-adce", "-gvn", "-instcombine", bc_file, "-o", output_file)
  pkt_hint_file.unlink

end

def main(bc_file, bc_base, output_file)
  input_file = bc_file
  counter = 0
  while true do
    Dir.glob("pkt*.bc*").each { |e| FileUtils.remove_entry_secure(e) }
    generate_test_cases(input_file)
    res = Dir.glob("pkt*.bc").map do |pkt_file| 
      test_pkt_bc(bc_base, pkt_file)
    end.compact
    break if res.empty?
    new_file =  "%s.bc.%d" % [File.basename(bc_file, ".bc"), counter]
    merge_packet(input_file, res, new_file)
    counter += 1
    input_file = new_file
  end

  if input_file != bc_file
    File.rename(input_file, output_file)
  else
    FileUtils.cp(input_file, output_file)
  end
end

bc_file = File::expand_path(ARGV[0])
output_file = File::expand_path(ARGV[1])
bc_base = ARGV.length > 2 ? File::expand_path(ARGV[2]) : ""

previous_dir = Dir.pwd
working_dir = "hint-%s" % File.basename(bc_file, ".bc")

begin
  Dir.mkdir(working_dir)
  Dir.chdir(working_dir)
  main(bc_file, bc_base, output_file)
ensure
  Dir.chdir(previous_dir)
  FileUtils.remove_entry_secure(working_dir, :force => true)
end
