// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-reschedule=0 -mllvm -polly-postopts=0 -mllvm -debug-only=polly-ast                                      -o /dev/null %s 2>&1 > /dev/null | FileCheck %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-reschedule=0 -mllvm -polly-postopts=0 -mllvm -debug-only=polly-ast -mllvm -polly-pragma-ignore-depcheck -o /dev/null %s 2>&1 > /dev/null | FileCheck %s --check-prefix=IGNOREDEPCHECK

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


// IGNOREDEPCHECK: for (int c0 = 0; c0 < p_1; c0 += 1) {
// IGNOREDEPCHECK:   for (int c1 = 0; c1 < p_0; c1 += 1)
// IGNOREDEPCHECK:     Stmt4(c1, c0);
// IGNOREDEPCHECK: }
