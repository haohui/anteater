// Copy propagation for symbolic packet
// Assume there is only one assertion 

#include <llvm/Pass.h>
#include <llvm/PassManager.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Module.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Instructions.h>
#include <llvm/Constants.h>
#include <llvm/Support/CommandLine.h>

#include <set>
#include <map>
#include <fstream>

#include <boost/tokenizer.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/pending/disjoint_sets.hpp>

namespace {

using namespace llvm;

static cl::opt<std::string>
g_mergePacketHint("merge-packet-hint", llvm::cl::desc("Hint to merge identical packets"), llvm::cl::value_desc("filename"));

class MergePacket : public ModulePass {
 public:
  MergePacket();
  const char * getPassName() const { return "Packet Propagation"; }
  bool runOnModule(Module & M);
  static char ID;
 private:
  typedef std::map<std::string, int> RankMap;
  typedef std::map<std::string, std::string> ParentMap;
  typedef boost::disjoint_sets<boost::associative_property_map<RankMap> , boost::associative_property_map<ParentMap> > DisjointSet;
  void buildDisjointSet(DisjointSet & set, std::set<std::string> & packets, const std::string &hint_file);
};
 
MergePacket::MergePacket()
    : ModulePass(ID)
{}

char MergePacket::ID = 0;

bool MergePacket::runOnModule(Module & M) {
  RankMap underlying_rank_map;
  ParentMap underlying_parent_map;
  boost::associative_property_map<RankMap> rank_map(underlying_rank_map);
  boost::associative_property_map<ParentMap> parent_map(underlying_parent_map);
  
  boost::disjoint_sets<boost::associative_property_map<RankMap> , boost::associative_property_map<ParentMap> > disjoint_set(rank_map, parent_map);

  std::set<std::string> packets;

  buildDisjointSet(disjoint_set, packets, g_mergePacketHint);
  
  for (std::set<std::string>::const_iterator it = packets.begin(), end = packets.end(); it != end; ++it) {
    GlobalVariable * P1 = M.getGlobalVariable(*it);
    GlobalVariable * P2 = M.getGlobalVariable(disjoint_set.find_set(*it));
    if (P1 != P2 && P1 && P2) {
      P1->replaceAllUsesWith(P2);
    }
  }
  return true;
}

void MergePacket::buildDisjointSet(DisjointSet & set, std::set<std::string> & packets, const std::string &hint_file) {
  std::ifstream file(hint_file.c_str());

  std::string line;
  std::ios::sync_with_stdio(false);
  
  while(getline(file, line)) {
    boost::char_separator<char> sep(",");
    typedef boost::tokenizer<boost::char_separator<char> > tok_t;
    tok_t tok(line, sep);
    tok_t::iterator it = tok.begin();
    std::string pkt1 = *it;
    std::string pkt2 = *(++it);
    if (!packets.count(pkt1)) {
      packets.insert(pkt1);
      set.make_set(pkt1);
    }
    
    if (!packets.count(pkt2)) {
      packets.insert(pkt2);
      set.make_set(pkt2);
    }
    
    set.union_set(pkt1, pkt2);
  }

  set.normalize_sets(packets.begin(), packets.end());
}

RegisterPass<MergePacket> X("merge-packet", "Copy Propagation for Symbolic Packet",
                      false /* Only looks at CFG */,
                      false /* Analysis Pass */);

}
