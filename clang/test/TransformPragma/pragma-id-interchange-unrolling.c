// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -fno-unroll-loops -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR --match-full-lines %s
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -debug-only=polly-ast -mllvm -polly-use-llvm-names -o /dev/null %s 2>&1 > /dev/null | FileCheck --check-prefix=AST %s
// RUN: %clang_cc1 -flegacy-pass-manager -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -o - %s | FileCheck --check-prefix=TRANS %s
// RUN: %clang                           -DMAIN                                   -std=c99            -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -o %t_pragma_id_interchange%exeext
// RUN: %t_pragma_id_interchange%exeext | FileCheck --check-prefix=RESULT %s

__attribute__((noinline))
void pragma_id_interchange_unrolling(double C[static const restrict 256][128], double A[static const restrict 128][256]) {
#pragma clang loop(i) unrolling factor(4)
#pragma clang loop(j) unrolling factor(2)
#pragma clang loop(i,j) interchange permutation(j,i)
#pragma clang loop id(i)
	for (int i = 0; i < 256; i += 1)
#pragma clang loop id(j)
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
  memset(C, 0, sizeof(C));
  A[1][2] = 42;
  pragma_id_interchange_unrolling(C,A);
  printf("(%0.0f)\n", C[2][1]);
  return 0;
}
#endif


// PRINT-LABEL: void pragma_id_interchange_unrolling(double C[const restrict static 256][128], double A[const restrict static 128][256]) __attribute__((noinline)) {
// PRINT-NEXT:  #pragma clang loop(i) unrolling factor(4)
// PRINT-NEXT:  #pragma clang loop(j) unrolling factor(2)
// PRINT-NEXT:  #pragma clang loop(i, j) interchange permutation(j, i)
// PRINT-NEXT:  #pragma clang loop id(i)
// PRINT-NEXT:  	for (int i = 0; i < 256; i += 1)
// PRINT-NEXT:  #pragma clang loop id(j)
// PRINT-NEXT:  		for (int j = 0; j < 128; j += 1)
// PRINT-NEXT:  			C[i][j] = A[j][i] + i + j;
// PRINT-NEXT:  }


// IR-LABEL: define dso_local void @pragma_id_interchange_unrolling([128 x double]* noalias align 8 dereferenceable(262144) %C, [256 x double]* noalias align 8 dereferenceable(262144) %A) #0 {
// IR:         br label %for.cond1, !llvm.loop !2
// IR:         br label %for.cond, !llvm.loop !9
//
// IR: !2 = distinct !{!2, !3, !4, !5}
// IR: !3 = !{!"llvm.loop.disable_nonforced"}
// IR: !4 = !{!"llvm.loop.id", !"j"}
// IR: !5 = !{!"llvm.loop.interchange.followup_interchanged", !6}
// IR: !6 = distinct !{!6, !3, !4, !7, !8}
// IR: !7 = !{!"llvm.loop.unroll.enable", i1 true}
// IR: !8 = !{!"llvm.loop.unroll.count", i3 2}
// IR: !9 = distinct !{!9, !3, !10, !11, !12, !13, !14}
// IR: !10 = !{!"llvm.loop.id", !"i"}
// IR: !11 = !{!"llvm.loop.interchange.enable", i1 true}
// IR: !12 = !{!"llvm.loop.interchange.depth", i32 2}
// IR: !13 = !{!"llvm.loop.interchange.permutation", i32 1, i32 0}
// IR: !14 = !{!"llvm.loop.interchange.followup_interchanged", !15}
// IR: !15 = distinct !{!15, !3, !10, !7, !16}
// IR: !16 = !{!"llvm.loop.unroll.count", i4 4}


// AST: if (1)
// AST: for (int c0 = 0; c0 <= 127; c0 += 2) {
// AST: 	for (int c1 = 0; c1 <= 255; c1 += 4) {
// AST: 		Stmt_for_body4(c1, c0);
// AST: 		Stmt_for_body4(c1 + 1, c0);
// AST: 		Stmt_for_body4(c1 + 2, c0);
// AST: 		Stmt_for_body4(c1 + 3, c0);
// AST: 	}
// AST: 	for (int c1 = 0; c1 <= 255; c1 += 4) {
// AST: 		Stmt_for_body4(c1, c0 + 1);
// AST: 		Stmt_for_body4(c1 + 1, c0 + 1);
// AST: 		Stmt_for_body4(c1 + 2, c0 + 1);
// AST: 		Stmt_for_body4(c1 + 3, c0 + 1);
// AST: 	}
// AST: }
// AST: else
// AST:   {  /* original code */ }


// TRANS-LABEL: @pragma_id_interchange_unrolling
// TRANS:       polly.loop_header32:
// TRANS:         store
// TRANS:         store
// TRANS:         store
// TRANS:         store
// TRANS:       polly.loop_header71:
// TRANS:         store
// TRANS:         store
// TRANS:         store
// TRANS:         store


// RESULT: (45)
