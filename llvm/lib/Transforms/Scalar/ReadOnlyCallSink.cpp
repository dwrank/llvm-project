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
#include "llvm/ADT/Statistic.h"
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

      LLVM_DEBUG(dbgs() << "ReadOnlyCallSink: candidate: " << *CB << "\n");

      // TODO: determine which successor(s) need the result, verify sinking
      // is profitable, then perform SplitBlock + conditional branch insertion.
    }
  }

  return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
