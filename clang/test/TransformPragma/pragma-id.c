// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1 -flegacy-pass-manager -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR %s

void pragma_id(double *A, int N) {
#pragma clang loop id(myloop)
  for (int i = 0; i < N; i += 1)
    A[i] = A[i] + 1;
}


// PRINT-LABEL: void pragma_id(double *A, int N) {
// PRINT-NEXT:  #pragma clang loop id(myloop)
// PRINT-NEXT:    for (int i = 0; i < N; i += 1)
// PRINT-NEXT:      A[i] = A[i] + 1;
// PRINT-NEXT:  }


// IR-LABEL: define dso_local void @pragma_id(double* %A, i32 %N) #0 {
// IR:         br label %for.cond, !llvm.loop !2
//
// IR:         !2 = distinct !{!2, !3}
// IR:         !3 = !{!"llvm.loop.id", !"myloop"}
