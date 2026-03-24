# Ch6 depends on JIT enabled.
if(NOT MLIR_ENABLE_EXECUTION_ENGINE)
  return()
endif()


# For a better template to copy, see examples/standalone
include_directories(include)
add_subdirectory(include)

set(LLVM_LINK_COMPONENTS
  Core
  Support
  nativecodegen
  OrcJIT
  )

set(LLVM_TARGET_DEFINITIONS mlir/ToyCombine.td)
mlir_tablegen(ToyCombine.inc -gen-rewriters)
add_public_tablegen_target(MyToyCombineIncGen)

add_toy_chapter(mytoyc
  toyc.cpp
  parser/AST.cpp
  mlir/MLIRGen.cpp
  mlir/Dialect.cpp
  mlir/LowerToAffineLoops.cpp
  mlir/LowerToLLVM.cpp
  mlir/ShapeInferencePass.cpp
  mlir/ToyCombine.cpp

  DEPENDS
  MyToyOpsIncGen
  MyToyCombineIncGen
  MyToyShapeInferenceInterfaceIncGen

  )
include_directories(include/)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include/)
target_link_libraries(mytoyc
  PRIVATE
  MLIRAnalysis
  MLIRBuiltinToLLVMIRTranslation
  MLIRCallInterfaces
  MLIRCastInterfaces
  MLIRExecutionEngine
  MLIRFunctionInterfaces
  MLIRIR
  MLIRLLVMCommonConversion
  MLIRLLVMDialect
  MLIRLLVMToLLVMIRTranslation
  MLIRMemRefDialect
  MLIRParser
  MLIRPass
  MLIRRegisterAllDialects
  MLIRRegisterAllExtensions
  MLIRRegisterAllPasses
  MLIRSideEffectInterfaces
  MLIRSupport
  MLIRTargetLLVMIRExport
  MLIRTransforms
)
