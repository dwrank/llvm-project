//===-- ReadOnlyCallSink.h - Sink read-only calls past branches -*- C++ -*-===//
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

#ifndef LLVM_TRANSFORMS_SCALAR_READONLYCALLSINK_H
#define LLVM_TRANSFORMS_SCALAR_READONLYCALLSINK_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class Function;

/// Sink read-only calls past conditional branches when their results are only
/// needed on a subset of outgoing paths.
class ReadOnlyCallSinkPass : public PassInfoMixin<ReadOnlyCallSinkPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_SCALAR_READONLYCALLSINK_H
