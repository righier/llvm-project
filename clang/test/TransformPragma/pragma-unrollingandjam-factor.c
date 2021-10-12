// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -disable-legacy-loop-transformations -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -disable-legacy-loop-transformations -emit-llvm -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -disable-legacy-loop-transformations -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -mllvm -debug-only=polly-ast -o /dev/null %s 2>&1 > /dev/null | FileCheck --check-prefix=AST %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -disable-legacy-loop-transformations -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -o - %s | FileCheck --check-prefix=TRANS %s
// RUN: %clang -DMAIN -std=c99 -disable-legacy-loop-transformations -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -o %t_pragma_pack%exeext
// RUN: %t_pragma_pack%exeext | FileCheck --check-prefix=RESULT %s


__attribute__((noinline))
void pragma_unrollingandjam(double C[const restrict static 128][128], double A[const restrict static 256]) {
  #pragma clang loop unrollingandjam factor(4)
  for (int i = 0; i < 128; i += 1)
    for (int j = 0; j < 128; j += 1)
      C[i][j] = A[i+j] + i + j;
}

#ifdef MAIN
#include <stdio.h>
#include <string.h>
int main() {
  double C[128][128];
  double A[256];
  memset(A, 0, sizeof(A));
  A[2] = 42;
  pragma_unrollingandjam(C,A);
  // C[1][1] = A[1+1] + 1 + 1 = 42 + 1 + 1 = 44
  printf("(%0.0f)\n", C[1][1]);
  return 0;
}
#endif

// PRINT-LABEL: void pragma_unrollingandjam(double C[const restrict static 128][128], double A[const restrict static 256]) __attribute__((noinline)) {
// PRINT-NEXT:  #pragma clang loop unrollingandjam factor(4)
// PRINT-NEXT:  for (int i = 0; i < 128; i += 1)
// PRINT-NEXT:  	for (int j = 0; j < 128; j += 1)
// PRINT-NEXT:  		C[i][j] = A[i + j] + i + j;
// PRINT-NEXT:  }


// IR-LABEL: define dso_local void @pragma_unrollingandjam([128 x double]* noalias align 8 dereferenceable(131072) %C, double* noalias align 8 dereferenceable(2048) %A) #0 {
// IR:         br label %for.cond1, !llvm.loop !2
// IR:         br label %for.cond, !llvm.loop !4
//
// IR: !2 = distinct !{!2, !3}
// IR: !3 = !{!"llvm.loop.disable_nonforced"}
// IR: !4 = distinct !{!4, !3, !5, !6}
// IR: !5 = !{!"llvm.loop.unroll_and_jam.enable", i1 true}
// IR: !6 = !{!"llvm.loop.unroll_and_jam.count", i4 4}


// AST: if (1)
// AST:   for (int c0 = 0; c0 <= 127; c0 += 4) {
// AST: 	for (int c1 = 0; c1 <= 127; c1 += 1) {
// AST: 		Stmt_for_body4(c0, c1);
// AST: 		Stmt_for_body4(c0 + 1, c1);
// AST: 		Stmt_for_body4(c0 + 2, c1);
// AST: 		Stmt_for_body4(c0 + 3, c1);
// AST: 	}
// AST:   }
// AST: else
// AST:   {  /* original code */ }


// TRANS-LABEL: @pragma_unrollingandjam
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add


// RESULT: (44)
