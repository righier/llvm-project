#!/bin/bash

mkdir -p ./build
mkdir -p ./build-install

pushd ./build

cmake -G Ninja \
	-DCMAKE_C_COMPILER=clang \
	-DCMAKE_CXX_COMPILER=clang++ \
	-DCMAKE_CXX_STANDARD=14 \
	-DCMAKE_INSTALL_PREFIX=/home/ermanno/thesis/llvm-project/build-install \
	-DCMAKE_BUILD_TYPE=Release \
	-DLLVM_ENABLE_ASSERTIONS=ON \
	-DLLVM_USE_LINKER=lld \
	-DLLVM_TARGETS_TO_BUILD=X86 \
	-DLLVM_ENABLE_PROJECTS='clang;polly' \
	../llvm

cmake --build .

cmake --build . --target check-polly

cmake --install .

popd
