#!/bin/bash
cmake -S llvm -B build -G Ninja \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
    -DLLVM_ENABLE_PROJECTS="clang;lld;mlir" \
    -DCLANG_DEFAULT_LINKER=lld \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_TARGETS_TO_BUILD="X86;NVPTX" \
    -DLLVM_OPTIMIZED_TABLEGEN=1 \
    -DLLVM_ENABLE_BINDINGS=0 \
    -DLLVM_ENABLE_LLD=1 \
    -DCLANG_DEFAULT_CXX_STDLIB=libc++ \
    -DLLVM_ENABLE_LIBCXX=1 \
    -DLLVM_CCACHE_BUILD=1 \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind"
#-DCMAKE_INSTALL_PREFIX=/home/drank/dev/compilers/llvm
