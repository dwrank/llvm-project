#!/bin/sh
alias b='ninja -C build mytoyc'
alias r='build/bin/mytoyc $@'
alias r1='build/bin/mytoyc tests/input/ch1.toy -emit=ast'
alias r2='build/bin/mytoyc tests/input/ch2.toy -emit=mlir'
