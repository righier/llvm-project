// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -mllvm -debug-only=polly-ast -o /dev/null %s 2>&1 > /dev/null | FileCheck --check-prefix=AST %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -o - %s | FileCheck --check-prefix=TRANS %s
// RUN: %clang -DMAIN -std=c99 -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -o %t_pragma_pack%exeext
// RUN: %t_pragma_pack%exeext | FileCheck --check-prefix=RESULT %s

__attribute__((noinline))
void pragma_pack(double C[const restrict static 256][128], double A[const restrict static 128][256]) {
  for (int i = 0; i < 256; i += 1)
    #pragma clang loop pack array(A)
    for (int j = 0; j < 128; j += 1)
      C[i][j] = A[j][i] + i + j;
}

#ifdef MAIN
#include <stdio.h>
#include <string.h>
int main() {
  double C[256][128];
  double A[128][256];
  memset(A, 0, sizeof(A));
  A[1][2] = 42;
  pragma_pack(C,A);
  printf("(%0.0f)\n", C[2][1]);
  return 0;
}
#endif


// PRINT-LABEL: void pragma_pack(double C[const restrict static 256][128], double A[const restrict static 128][256]) __attribute__((noinline)) {
// PRINT-NEXT:     for (int i = 0; i < 256; i += 1)
// PRINT-NEXT: #pragma clang loop pack array(A)
// PRINT-NEXT:         for (int j = 0; j < 128; j += 1)
// PRINT-NEXT:             C[i][j] = A[j][i] + i + j;
// PRINT-NEXT: }


// IR-LABEL: define dso_local void @pragma_pack([128 x double]* noalias align 8 dereferenceable(262144) %C, [256 x double]* noalias align 8 dereferenceable(262144) %A) #0 {
// IR:         %A.addr = alloca [256 x double]*, align 8
// IR:         %5 = load double, double* %arrayidx5, align 8, !llvm.access !2
//
// IR: !2 = distinct !{!"A"}
// IR: !3 = distinct !{!3, !4, !5, !6, !7}
// IR: !4 = !{!"llvm.loop.disable_nonforced"}
// IR: !5 = !{!"llvm.data.pack.enable", i1 true}
// IR: !6 = !{!"llvm.data.pack.array", !2}
// IR: !7 = !{!"llvm.data.pack.allocate", !"alloca"}


// AST: if (1)
// AST:     for (int c0 = 0; c0 <= 255; c0 += 1) {
// AST:       for (int c3 = 0; c3 <= 127; c3 += 1)
// AST:         CopyStmt_0(c0, 0, c3);
// AST:       for (int c1 = 0; c1 <= 127; c1 += 1)
// AST:         Stmt_for_body4(c0, c1);
// AST:     }
// AST: else
// AST:     {  /* original code */ }


// TRANS-LABEL: @pragma_pack
// TRANS: Packed_MemRef_


// RESULT: (45)
