#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "vector"
#include "unordered_map"
#include "set"
#include <fstream>
#include <sstream>
#include <queue>

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/DebugInfoMetadata.h"

using namespace llvm;
using namespace std;
unordered_map<string, vector<pair<string, uint64_t>>> StructLayoutMap;
namespace {
  struct CGraph : public ModulePass {
    static char ID;
    CGraph() : ModulePass(ID) {}
    bool runOnModule(Module &M) override;
  }; 
}

string getName (Value *v) {
  if (v->hasName()) return v->getName().str();
  else {
    string temp;
    raw_string_ostream os(temp);
    v->printAsOperand(os, false);
    return os.str();
  }
}

vector<string> resolveRegisterValue(const string &reg, unordered_map<string, vector<string>> &RegisterValueMap) {
  vector<string> result;
  queue<string> q;
  set<string> visited;

  q.push(reg);
  while (!q.empty()) {
    string cur = q.front(); q.pop();
    if (!visited.insert(cur).second) continue;

    if (RegisterValueMap.count(cur)) {
      for (const string &val : RegisterValueMap[cur])
        q.push(val);
    } else {
      result.push_back(cur);
    }
  }
  return result;
}

bool areGEPsAliased(const string &a, const string &b,
                    unordered_map<string, set<pair<string, vector<string>>>> &GEPMap,
                    unordered_map<string, vector<string>> &RegisterValueMap) {
  if (!GEPMap.count(a) || !GEPMap.count(b)) return false;

  const auto &aSet = GEPMap[a];
  const auto &bSet = GEPMap[b];

  for (const auto &[baseA, pathA] : aSet) {
    vector<string> resolvedA = resolveRegisterValue(baseA, RegisterValueMap);

    for (const auto &[baseB, pathB] : bSet) {
      vector<string> resolvedB = resolveRegisterValue(baseB, RegisterValueMap);

      // 교집합이 존재하는지 확인
      bool aliasBase = false;
      for (const string &x : resolvedA) {
        if (resolvedB.size()) {
          aliasBase = true;
          break;
        }
      }

      if (!aliasBase) continue;

      // path 비교
      if (pathA.size() != pathB.size()) continue;
      bool samePath = true;
      for (size_t i = 0; i < pathA.size(); ++i) {
        if (pathA[i] != pathB[i]) {
          samePath = false;
          break;
        }
      }

      if (samePath) return true;
    }
  }

  return false;
}


void handleGEPInstruction(Instruction &I, 
                          unordered_map<string, vector<string>> &RegisterValueMap,
                          unordered_map<string, set<pair<string, vector<string>>>> &GEPMap) {
  auto *gep = dyn_cast<GetElementPtrInst>(&I);
  if (!gep) return;

  string result = getName(&I);
  string baseName = getName(gep->getPointerOperand());

  // resolve all possible base aliases
  vector<string> bases = resolveRegisterValue(baseName, RegisterValueMap);

  for (const string &base : bases) {
    vector<string> fieldPath;
    Type *currType = gep->getPointerOperandType()->getPointerElementType();

    for (auto it = gep->idx_begin(); it != gep->idx_end(); ++it) {
      Value *idxVal = it->get();

      if (auto *ci = dyn_cast<ConstantInt>(idxVal)) {
        uint64_t idx = ci->getZExtValue();

        if (StructType *st = dyn_cast<StructType>(currType)) {
          if (idx < st->getNumElements()) {
            fieldPath.push_back("field" + to_string(idx));
            currType = st->getElementType(idx);
          }
        } 
        else {
          fieldPath.push_back("idx" + to_string(idx));
        }
      } 
    }

    // Insert into GEP map
    GEPMap[result].insert({base, fieldPath});
    RegisterValueMap[result].push_back(base);
  }
}

void handleStoreInstruction(Instruction &I, unordered_map<string, vector<string>> &RegisterValueMap,
                            unordered_map<string, set<pair<string, vector<string>>>> &GEPMap, Module &M) {
  Value *val = I.getOperand(0);
  Value *r = I.getOperand(1);

  string rName, valName;

  rName = getName(r);

  if (Function *func = dyn_cast<Function>(val->stripPointerCasts())) {
    valName = func->getName().str();
  } 
  else if (val->hasName()) {
    valName = val->getName().str();
  } 
  else {
    string temp;
    raw_string_ostream os(temp);
    val->printAsOperand(os, false);
    valName = os.str();
  }

  RegisterValueMap[rName] = {valName};

  for (auto &[key, _] : GEPMap) {
    if (key != rName && areGEPsAliased(rName, key, GEPMap, RegisterValueMap)) {
      RegisterValueMap[key] = {valName};
    }
  }
}

void handleLoadInstruction(Instruction &I, unordered_map<string, vector<string>> &RegisterValueMap,
                           unordered_map<string, set<pair<string, vector<string>>>> &GEPMap) {
  LoadInst *load = dyn_cast<LoadInst>(&I);
  if (!load) return;

  Value *src = load->getPointerOperand();
  string srcName, destName;

  srcName = getName(src);
  destName = getName(&I);

  vector<string> resolved;
  
  if (RegisterValueMap.count(srcName)) {
    resolved = resolveRegisterValue(srcName, RegisterValueMap);
  }

  for (auto &[key, _] : GEPMap) {
    if (key != srcName && areGEPsAliased(srcName, key, GEPMap, RegisterValueMap)) {
      resolved = resolveRegisterValue(key, RegisterValueMap);
      break;
    }
  }

  if (!resolved.empty()) {
    RegisterValueMap[destName] = resolved;
  }
  if (dyn_cast<ConstantExpr>(src)) {
    string fieldName;
    ConstantExpr *ce = dyn_cast<ConstantExpr>(src);
    if (ce->getOpcode() == Instruction::GetElementPtr) {
      Type *structType = ce->getOperand(0)->getType()->getPointerElementType();
      if (auto *st = dyn_cast<StructType>(structType)) {
        if (st->hasName() && StructLayoutMap.count(st->getName().str())) {

          string structName = st->getName().str();
          if (ce->getNumOperands() >= 3) {
            if (auto *ci = dyn_cast<ConstantInt>(ce->getOperand(2))) {
              uint64_t index = ci->getZExtValue();
              fieldName = StructLayoutMap[structName][index].first;
              RegisterValueMap[getName(&I)] = {fieldName};
            }
          }
        }
      }
    }
  }

  return;
}

void addCall(vector<pair<string, vector<string>>> &CallGraph,
              const string &caller, const vector<string> &calleeSet) {
  for (auto &entry : CallGraph) {
    if (entry.first == caller) {
      // 중복 방지
      if (entry.second.empty()) {
        entry.second.insert(entry.second.end(), calleeSet.begin(), calleeSet.end());
      } 
      else {
        for (const string &callee : calleeSet) {
          if (find(entry.second.begin(), entry.second.end(), callee) == entry.second.end()) {
            entry.second.push_back(callee);
          }
        }
      }
      return;
  }
  }

  if (calleeSet.empty()) {
    CallGraph.push_back(make_pair(caller, vector<string>()));
  } else {
    vector<string> callees(calleeSet.begin(), calleeSet.end());
    CallGraph.push_back(make_pair(caller, callees));
  }
}

void handleCallInstruction(Instruction &I, unordered_map<string, vector<string>> &RegisterValueMap,vector<pair<string, vector<string>>> &CallGraph, const string &callerName) {
  CallInst *call = dyn_cast<CallInst>(&I);
  if (!call) return;

  Value *called = call->getCalledOperand()->stripPointerCasts();
  vector<string> calleeName;

  if (Function *func = dyn_cast<Function>(called)) {
    calleeName.push_back(func->getName().str());
  } else {
    string reg = getName(called);
    calleeName = resolveRegisterValue(reg, RegisterValueMap);
  }

  if (!calleeName.empty()) {
    addCall(CallGraph, callerName, calleeName);
  }
}

void buildStructLayoutMap(Module &M) {
  SmallPtrSet<const MDNode *, 32> visited;

  for (const NamedMDNode &nmd : M.named_metadata()) {
    for (unsigned i = 0; i < nmd.getNumOperands(); ++i) {
      MDNode *node = nmd.getOperand(i);
      queue<MDNode *> queue;
      
      queue.push(node);
      
      while (!queue.empty()) {
        MDNode *curr = queue.front();
        queue.pop();

        if (!visited.insert(curr).second) continue;

        // Check if the current node is a composite type (struct)
        if (auto *CT = dyn_cast<DICompositeType>(curr)) {
          string structName = CT->getName().str();
          if (structName.empty()) continue;
          else {
            structName = "struct." + structName;
          }

          DINodeArray fields = CT->getElements();
          vector<pair<string, uint64_t>> layout;

          for (auto *el : fields) {
            if (auto *field = dyn_cast<DIDerivedType>(el)) {
              string fieldName = field->getName().str();
              uint64_t offset = field->getOffsetInBits() / 8;
              layout.push_back(make_pair(fieldName, offset));
            }
          }

          StructLayoutMap[structName] = layout;
        }

        // Traverse children recursively
        for (unsigned op = 0; op < curr->getNumOperands(); ++op) {
          if (auto *opNode = dyn_cast_or_null<MDNode>(curr->getOperand(op))) queue.push(opNode);
        }
      }
    }
  }
}

void printCallGraph(Module &M, const vector<pair<string, vector<string>>> &CallGraph) {
  for (const auto &[caller, callees] : CallGraph) {
    Function *F = M.getFunction(caller);
    // 외부 함수 skip
    if (!F || F->isDeclaration()) continue;
    errs() << caller << ": ";
    for (const string &callee : callees) {
      if (callee.compare(0, 5, "llvm.") == 0) continue;
      errs() << callee << " ";
    }
  
    errs() << "\n";
  }
}

void mergeRegisterValueMap(unordered_map<string, vector<string>> &MergedRegisterValueMap,
                            const unordered_map<string, vector<string>> &RegisterValueMap) {
  for (const auto &entry : RegisterValueMap) {
    const string &key = entry.first;
    const vector<string> &values = entry.second;

    if (MergedRegisterValueMap.count(key)) {
      for (const string &value : values) {
        if (find(MergedRegisterValueMap[key].begin(), MergedRegisterValueMap[key].end(), value) == MergedRegisterValueMap[key].end()) {
          MergedRegisterValueMap[key].push_back(value);
        }
      }
    } else {
      MergedRegisterValueMap[key] = values;
    }
  }
}
void mergeGEPMap(unordered_map<string, set<pair<string, vector<string>>>> &MergedGEPMap,
                 const unordered_map<string, set<pair<string, vector<string>>>> &GEPMap) {
  for (const auto &entry : GEPMap) {
    const string &key = entry.first;
    const set<pair<string, vector<string>>> &values = entry.second;

    if (MergedGEPMap.count(key)) {
      for (const auto &[base, path] : values) {
        MergedGEPMap[key].insert({base, path});
      }
    } else {
      MergedGEPMap[key] = values;
    }
  }
}

bool CGraph::runOnModule(Module &M) {
  vector<pair<string, vector<string>>> CallGraph;
  buildStructLayoutMap(M);

  for (Function &F : M) {
    //DEBUG
    // errs() << "Function: " << F.getName() << "\n";

    string callerName = F.getName().str();
    unordered_map<string, vector<string>> MergedRegisterValueMap;
    unordered_map<string, vector<string>> RegisterValueMap;

    //TODO: Erase GEPMap and integrate it into RegisterValueMap
    unordered_map<string, set<pair<string, vector<string>>>> MergedGEPMap;
    unordered_map<string, set<pair<string, vector<string>>>> GEPMap;

    addCall(CallGraph, callerName, vector<string>());
    for (BasicBlock &BB : F) {
      RegisterValueMap = MergedRegisterValueMap;
      GEPMap = MergedGEPMap;
      // DEBUG
      // errs() << "  Basic Block: " << BB.getName() << "\n";
      
      // Iterate through all instructions in the basic block
      for (Instruction &I : BB) {
        switch (I.getOpcode()) {
          case Instruction::Store: {
            handleStoreInstruction(I, RegisterValueMap, GEPMap, M);
            break;
          }
          case Instruction::Load: {
            handleLoadInstruction(I, RegisterValueMap, GEPMap);
            break;
          }
          case Instruction::GetElementPtr: {
            handleGEPInstruction(I, RegisterValueMap, GEPMap);
            break;
          }
          case Instruction::Call: {
            handleCallInstruction(I, RegisterValueMap, CallGraph, callerName);
            break;
          }
          default:{
            break;
          }
        }
        // errs() << "    I: ";
        // I.print(errs());
        // errs() << "\n";
      }
      // Merge RegisterValueMap into MergedRegisterValueMap
      mergeRegisterValueMap(MergedRegisterValueMap, RegisterValueMap);
      // Merge GEPMap into MergedGEPMap
      mergeGEPMap(MergedGEPMap, GEPMap);
    }
  }

  // Print the call graph
  printCallGraph(M, CallGraph);
  return false;
}

char CGraph::ID = 0;
static RegisterPass<CGraph> X("cgraph", "Call Graph Pass",
                             true,
                             true);


static void registerCGraphPass(const PassManagerBuilder &, legacy::PassManagerBase &PM)
{
	PM.add(new CGraph());
}

//static RegisterStandardPasses Y(
//    PassManagerBuilder::EP_EarlyAsPossible,
//    registerCGraphPass);
static RegisterStandardPasses RegisterCGraphPass(
    PassManagerBuilder::EP_ModuleOptimizerEarly, registerCGraphPass);

static RegisterStandardPasses RegisterCGraphPass0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerCGraphPass);
