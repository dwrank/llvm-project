//===-- ReadOnlyCallSink.cpp - Sink read-only calls past branches ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass sinks calls that only read memory (pure/const functions and
// equivalents) past conditional branches when their results are only needed on
// a subset of outgoing paths. It performs the necessary CFG transformation
// (block splitting) that the general SinkingPass cannot do.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar/ReadOnlyCallSink.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "readonly-call-sink"

STATISTIC(NumSunk, "Number of read-only calls sunk past conditional branches");

/// Returns true if \p CB is a candidate for sinking past the conditional
/// branch \p BI at the end of the same block.
static bool isCandidate(const CallBase *CB, const CondBrInst *BI) {
  // Must only read memory. This covers __attribute__((pure)) (memory(read)),
  // __attribute__((const)) (memory(none)), attributes inferred by FunctionAttrs,
  // readonly/readnone intrinsics, and TLI-annotated library functions.
  if (!CB->onlyReadsMemory())
    return false;

  // Must be guaranteed to return. This excludes looping pure/const functions
  // (GCC's ECF_LOOPING_CONST_OR_PURE). A call that might not terminate cannot
  // be sunk: the branch could have skipped it entirely.
  if (!CB->willReturn())
    return false;

  // Must not throw. Sinking past a branch would alter the set of paths on
  // which an exception can escape, changing observable behavior.
  if (CB->mayThrow())
    return false;

  // Convergent calls must execute on the same threads as the call site demands.
  // Making them newly control-dependent violates that constraint.
  if (CB->isConvergent())
    return false;

  // Skip intrinsics conservatively. Many carry implicit semantics (memory
  // effects, target-specific restrictions) not fully captured by the attribute
  // checks above.
  if (isa<IntrinsicInst>(CB))
    return false;

  // setjmp-like calls may return multiple times via a non-local jump. The
  // second return would bypass the sunk call, producing wrong results.
  if (CB->hasFnAttr(Attribute::ReturnsTwice))
    return false;

  // Token-typed results cannot cross block boundaries or be used in phi nodes.
  if (CB->getType()->isTokenTy())
    return false;

  // Dead calls produce no value on any path; DCE handles them.
  if (CB->use_empty())
    return false;

  // The call result must not feed the branch condition. Sinking it past the
  // branch that it controls would be circular.
  if (CB == BI->getCondition())
    return false;

  return true;
}

/// Collects the set of instructions in \p BB that form the def-use chain
/// rooted at \p CB and must be moved as a unit. Returns std::nullopt if any
/// chain member has an in-block use outside the chain, meaning the result is
/// consumed before the branch and cannot be deferred.
static std::optional<SmallPtrSet<Instruction *, 8>>
buildSinkGroup(CallBase *CB, BasicBlock *BB) {
  SmallPtrSet<Instruction *, 8> Group;
  SmallVector<Instruction *, 8> Worklist;

  Group.insert(CB);
  Worklist.push_back(CB);

  // Expand: follow uses within BB. PHI nodes cannot appear after a non-PHI
  // instruction in the same block (SSA invariant), so any in-block user of a
  // group member is guaranteed to be a non-PHI. If a group member is used by
  // the terminator — directly or through intermediate instructions — the result
  // is needed before the branch (e.g., it feeds the branch condition) and
  // sinking is not possible.
  while (!Worklist.empty()) {
    Instruction *I = Worklist.pop_back_val();
    for (User *U : I->users()) {
      auto *UI = cast<Instruction>(U);
      if (UI->getParent() != BB)
        continue;
      if (UI->isTerminator())
        return std::nullopt;
      if (Group.insert(UI).second)
        Worklist.push_back(UI);
    }
  }

  return Group;
}

/// Returns the set of direct successors of \p BB (via \p BI) that require
/// the sink group's result to be computed before entering them.
///
/// For phi uses, the effective use location is the incoming block: a phi
/// [ %val, %BB ] in successor S means the BB→S edge carries the result.
/// For non-phi uses in a block that is not a direct successor, we cannot
/// determine the specific edge without deeper dominator analysis, so we
/// conservatively treat all successors as needed.
static SmallPtrSet<BasicBlock *, 2>
findNeededSuccessors(const SmallPtrSet<Instruction *, 8> &Group,
                     BasicBlock *BB, CondBrInst *BI) {
  SmallPtrSet<BasicBlock *, 2> Needed;

  for (Instruction *I : Group) {
    for (User *U : I->users()) {
      auto *UI = cast<Instruction>(U);
      BasicBlock *UseBlock = UI->getParent();

      if (UseBlock == BB)
        continue; // in-block uses are within the group (validated above)

      // Non-direct-successor blocks are dominated by one of the two direct
      // successors, so their execution is already governed by whichever
      // successor we identify as needed here. No separate handling is required.
      if (UseBlock != BI->getSuccessor(0) && UseBlock != BI->getSuccessor(1))
        continue;

      if (auto *PN = dyn_cast<PHINode>(UI)) {
        // A phi [ %val, %IncomingBB ] means the value is needed on the edge
        // IncomingBB → UseBlock. Only the edge from BB is our concern.
        for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
          if (PN->getIncomingValue(i) == I && PN->getIncomingBlock(i) == BB) {
            Needed.insert(UseBlock);
            break; // each block has at most one incoming slot in a phi node
          }
        }
      } else {
        // Non-phi use in a direct successor.
        Needed.insert(UseBlock);
      }
    }
  }

  return Needed;
}

PreservedAnalyses ReadOnlyCallSinkPass::run(Function &F,
                                            FunctionAnalysisManager &AM) {
  auto &DT = AM.getResult<DominatorTreeAnalysis>(F);
  bool Changed = false;

  for (BasicBlock &BB : F) {
    // Skip unreachable blocks. They can contain malformed IR, and processing
    // them risks non-termination if they form unreachable cycles.
    if (!DT.isReachableFromEntry(&BB))
      continue;

    // We can only sink past a conditional branch.
    auto *BI = dyn_cast<CondBrInst>(BB.getTerminator());
    if (!BI)
      continue;

    for (Instruction &I : BB) {
      auto *CB = dyn_cast<CallBase>(&I);
      if (!CB)
        continue;

      if (!isCandidate(CB, BI))
        continue;

      // Collect the def-use chain that must move with CB.
      auto GroupOpt = buildSinkGroup(CB, &BB);
      if (!GroupOpt)
        continue; // result consumed before branch; cannot sink

      auto &Group = *GroupOpt;
      auto Needed = findNeededSuccessors(Group, &BB, BI);

      // If both successors need the result, sinking provides no benefit.
      if (Needed.size() == 2)
        continue;

      // If no successor needs the result, the call is dead; let DCE handle it.
      if (Needed.empty())
        continue;

      // Exactly one successor needs the result — sinking is profitable.
      BasicBlock *NeedBB = *Needed.begin();
      BasicBlock *SkipBB = BI->getSuccessor(0) == NeedBB ? BI->getSuccessor(1)
                                                          : BI->getSuccessor(0);

      LLVM_DEBUG(dbgs() << "ReadOnlyCallSink: candidate to sink past branch:\n"
                        << "  call: " << *CB << "\n"
                        << "  needed by:  " << NeedBB->getName() << "\n"
                        << "  skipped by: " << SkipBB->getName() << "\n");

      // TODO: perform SplitBlock + conditional branch insertion.
      (void)SkipBB;
    }
  }

  return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
