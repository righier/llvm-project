// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -mllvm -debug-only=polly-ast -o /dev/null %s 2>&1 > /dev/null | FileCheck --check-prefix=AST --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -o - %s | FileCheck --check-prefix=TRANS %s
// RUN: %clang -DMAIN -std=c99 -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -o %t_pragma_reverse%exeext
// RUN: %t_pragma_reverse%exeext | FileCheck --check-prefix=RESULT --match-full-lines %s

__attribute__((noinline))
void pragma_id_reverse(int n, double A[n]) {
#pragma clang loop(myloop) reverse
#pragma clang loop id(myloop)
  for (int i = n - 1; i >= 0; i--)
    A[i] = A[i] + 1;
}

#ifdef MAIN
#include <stdio.h>
#include <string.h>
int main() {
	double A[2048];
	memset(A, 0, sizeof(A));
	A[2] = 42;
	pragma_id_reverse(2048,A);
	printf("(%0.0f)\n", A[2]);
	return 0;
}
#endif


// PRINT-LABEL: void pragma_id_reverse(int n, double A[n]) __attribute__((noinline)) {
// PRINT-NEXT:  #pragma clang loop(myloop) reverse
// PRINT-NEXT:  #pragma clang loop id(myloop)
// PRINT-NEXT:    for (int i = n - 1; i >= 0; i--)
// PRINT-NEXT:      A[i] = A[i] + 1;
// PRINT-NEXT:  }


// IR-LABEL: define dso_local void @pragma_id_reverse(i32 %n, double* %A) #0 {
// IR:         br label %for.cond, !llvm.loop !2
//
// IR: !2 = distinct !{!2, !3, !4, !5}
// IR: !3 = !{!"llvm.loop.disable_nonforced"}
// IR: !4 = !{!"llvm.loop.id", !"myloop"}
// IR: !5 = !{!"llvm.loop.reverse.enable", i1 true}


// AST: if (1)
// AST:   for (int c0 = -n + 1; c0 <= 0; c0 += 1)
// AST:     Stmt_for_body(-c0);
// AST: else
// AST:   {  /* original code */ }


// TRANS-LABEL: @pragma_id_reverse(
// TRANS:         %polly.indvar_next = add nsw i64 %polly.indvar, 1
// TRANS:         %exitcond.not = icmp eq i64 %polly.indvar, %smax
// TRANS:       polly.loop_preheader:
// TRANS:         %smax = call i64 @llvm.smax.i64(i64 %1, i64 0)


// RESULT: (43)
