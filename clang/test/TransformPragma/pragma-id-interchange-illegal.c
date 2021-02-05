// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-reschedule=0 -mllvm -polly-postopts=0 -mllvm -debug-only=polly-ast              %s -o /dev/null 2>&1 >/dev/null | FileCheck %s --check-prefix=CHECK
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-reschedule=0 -mllvm -polly-postopts=0 -debug-info-kind=limited -gno-column-info %s -o /dev/null 2>&1 >/dev/null | FileCheck %s --check-prefix=FAILURE

// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-reschedule=0 -mllvm -polly-postopts=0 -mllvm -polly-pragma-ignore-depcheck -mllvm -debug-only=polly-ast                                      %s  -o /dev/null  2>&1 >/dev/null | FileCheck %s --check-prefix=IGNOREDEPCHECK
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-reschedule=0 -mllvm -polly-postopts=0 -mllvm -polly-pragma-ignore-depcheck -debug-info-kind=limited -gno-column-info -Rpass=polly-opt-manual %s  -o /dev/null 2>&1 >/dev/null | FileCheck %s --check-prefix=WARN


void pragma_id_interchange(int n, int m, double C[m][n]) {
#pragma clang loop(i, j) interchange permutation(j, i)
#pragma clang loop id(i)
  for (int i = 0; i < m; i += 1)
#pragma clang loop id(j)
    for (int j = 0; j < n; j += 1)
      C[i][j] = C[j][i] + i + j;
}


// CHECK: for (int c0 = 0; c0 < p_0; c0 += 1) {
// CHECK:   for (int c1 = 0; c1 < p_1; c1 += 1)
// CHECK:     Stmt4(c0, c1);
// CHECK: }

// FAILURE: pragma-id-interchange-illegal.c:9:1: warning: not applying loop interchange: cannot ensure semantic equivalence due to possible dependency violations [-Wpass-failed=polly-opt-manual]
// FAILURE: #pragma clang loop(i, j) interchange permutation(j, i)
// FAILURE: ^
// FAILURE: 1 warning generated.


// IGNOREDEPCHECK: for (int c0 = 0; c0 < p_1; c0 += 1) {
// IGNOREDEPCHECK:   for (int c1 = 0; c1 < p_0; c1 += 1)
// IGNOREDEPCHECK:     Stmt4(c1, c0);
// IGNOREDEPCHECK: }

// WARN: pragma-id-interchange-illegal.c:9:1: remark: Could not verify dependencies for loop interchange; still applying because of -polly-pragma-ignore-depcheck [-Rpass=polly-opt-manual]
// WARN: #pragma clang loop(i, j) interchange permutation(j, i)
// WARN: ^
