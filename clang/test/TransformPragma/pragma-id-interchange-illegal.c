// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -o /dev/null -mllvm -debug-only=polly-ast %s 2>&1 > /dev/null | FileCheck %s

void pragma_id_interchange(int n, int m, double C[m][n]) {
#pragma clang loop(i, j) interchange permutation(j, i)
#pragma clang loop id(i)
  for (int i = 0; i < m; i += 1)
#pragma clang loop id(j)
    for (int j = 0; j < n; j += 1)
      C[i][j] = C[j][i] + i + j;
}


// CHECK:      for (int c0 = 0; c0 < p_0 + p_1 - 1; c0 += 1)
// CHECK-NEXT:   for (int c1 = max(0, -p_1 + c0 + 1); c1 <= min(p_0 - 1, c0); c1 += 1)
// CHECK-NEXT:     Stmt3(c1, c0 - c1);
