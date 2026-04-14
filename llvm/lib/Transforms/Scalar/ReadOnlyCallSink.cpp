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
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"

using namespace llvm;

PreservedAnalyses ReadOnlyCallSinkPass::run(Function &F,
                                            FunctionAnalysisManager &AM) {
  // TODO: implement read-only call sinking
  return PreservedAnalyses::all();
}
