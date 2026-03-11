# For a better template to copy, see examples/standalone
add_subdirectory(include)

set(LLVM_LINK_COMPONENTS
  Support
  )

add_toy_chapter(mytoyc
  toyc.cpp
  parser/AST.cpp
  mlir/MLIRGen.cpp
  mlir/Dialect.cpp

  DEPENDS
  MyToyOpsIncGen

  )
include_directories(include/)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include/)
target_link_libraries(mytoyc
  PRIVATE
  MLIRAnalysis
  MLIRFunctionInterfaces
  MLIRIR
  MLIRParser
  MLIRSideEffectInterfaces
  MLIRTransforms)
