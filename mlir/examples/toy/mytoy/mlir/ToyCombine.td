//===- ToyCombine.td - Pattern Match Optimizations for Toy -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines language-specific pattern match optimizations for Toy using
// Declarative Rewrite Rules (DRR) specified using TableGen records.
//
//===----------------------------------------------------------------------===//

#ifndef TOY_COMBINE
#define TOY_COMBINE

include "mlir/IR/PatternBase.td"
include "toy/Ops.td"

/// Note: The DRR definition used for defining patterns is shown below:
///
/// class Pattern<
///    dag sourcePattern, list<dag> resultPatterns,
///    list<dag> additionalConstraints = [],
//     list<dag> supplementalPatterns = [],
///    dag benefitsAdded = (addBenefit 0)
/// >;

//===----------------------------------------------------------------------===//
// Basic Pattern-Match and Rewrite
//===----------------------------------------------------------------------===//

// Reshape(Reshape(x)) = Reshape(x)
def ReshapeReshapeOptPattern : Pat<(ReshapeOp(ReshapeOp $arg)),
                                   (ReshapeOp $arg)>;

//===----------------------------------------------------------------------===//
// Pattern-Match and Rewrite using Native Code Call
//===----------------------------------------------------------------------===//

// Native Code Calls may be used for more complex transformations using inline
// C++ and C++ helper functions.

// Reshape(Constant(x)) = x'
def ReshapeConstant :
  NativeCodeCall<"$0.reshape(::llvm::cast<ShapedType>($1.getType()))">;
def FoldConstantReshapeOptPattern : Pat<
  (ReshapeOp:$res (ConstantOp $arg)),
  (ConstantOp (ReshapeConstant $arg, $res))>;

//===----------------------------------------------------------------------===//
// Pattern-Match and Rewrite with Constraints
//===----------------------------------------------------------------------===//

// DRR allows for constraint checking when the transformation is conditional
// on operand properties.

// Reshape(x) = x, where input and output shapes are identical
def TypesAreIdentical : Constraint<CPred<"$0.getType() == $1.getType()">>;
def RedundantReshapeOptPattern : Pat<
  (ReshapeOp:$res $arg), (replaceWithValue $arg),
  [(TypesAreIdentical $res, $arg)]>;

//===----------------------------------------------------------------------===//
// AddOp Pattern-Match and Rewrites
//===----------------------------------------------------------------------===//

// Add(Constant(x), y) = Add(y, Constant(x)), where y is not a constant.
// Moves constant operands to the RHS for canonicalization consistency.
// $_ in ConstantOp is it's attribute arg or value that stores the constant tensor,
// and does not need to be bound, since we just need 
def IsNotConstant : Constraint<CPred<"!$0.getDefiningOp<::mlir::toy::ConstantOp>()">>;
def MoveLHSConstToRHSOptPattern : Pat<
  (AddOp (ConstantOp:$lhs $_), $rhs),
  (AddOp $rhs, $lhs),
  [(IsNotConstant $rhs)]>;

// Add(x, x) = Mul(x, 2.0)
def CreateF64TwoAttr : NativeCodeCall<
    "::mlir::DenseElementsAttr::get("
    "::mlir::RankedTensorType::get({}, $_builder.getF64Type()),"
    "::llvm::ArrayRef<double>{2.0})">;
def SameValue : Constraint<CPred<"$0 == $1">>;
def ReplaceAddItselfWithMulBy2 : Pat<
  (AddOp $lhs, $rhs),
  (MulOp $lhs, (ConstantOp (CreateF64TwoAttr))),
  [(SameValue $lhs, $rhs)]>;

#endif // TOY_COMBINE
