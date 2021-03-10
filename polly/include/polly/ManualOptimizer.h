//===------ ManualOptimizer.h ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#ifndef POLLY_MANUALOPTIMIZER_H
#define POLLY_MANUALOPTIMIZER_H

#include "isl/isl-noexceptions.h"

namespace llvm {
class OptimizationRemarkEmitter;
}

namespace polly {
class Scop;
struct Dependences;

isl::schedule applyManualTransformations(Scop &S, isl::schedule Sched,
                                         const Dependences &D,
                                         llvm ::OptimizationRemarkEmitter *ORE);

} // namespace polly

#endif /* POLLY_MANUALOPTIMIZER_H */
