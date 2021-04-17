// RUN: %clang_cc1                        -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -fno-unroll-loops -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1                        -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -fno-unroll-loops -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -emit-llvm -o -         %s -disable-llvm-passes | FileCheck --check-prefix=IR %s
// RUN: %clang_cc1                        -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -fno-unroll-loops -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -emit-llvm -o /dev/null %s -mllvm -debug-only=polly-ast 2>&1 > /dev/null | FileCheck --check-prefix=AST %s
// RUN: %clang_cc1  -flegacy-pass-manager -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -fno-unroll-loops -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -emit-llvm -o -         %s | FileCheck --check-prefix=TRANS %s
// RUN: %clang                            -DMAIN                                   -std=c99 -fno-unroll-loops -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -ffast-math -march=native -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -o %t_pragma_pack%exeext
// RUN: %t_pragma_pack%exeext | FileCheck --check-prefix=RESULT %s

__attribute__((noinline))
void matmul(int M, int N, int K, double C[const restrict static M][N], double A[const restrict static M][K], double B[const restrict static K][N]) {
#pragma clang loop(i1) pack array(B) allocate(malloc) isl_redirect("{ Prefix[c,j,k]   -> [MyB[x,y] -> MyPacked_B[floord(y,8) mod 256,x mod 256,y mod 8]] }")
#pragma clang loop(j2) pack array(A) allocate(malloc) isl_redirect("{ Prefix[c,j,k,l] -> [MyA[x,y] -> MyPacked_A[floord(x,4) mod 16,y mod 256,x mod 4]] }")
#pragma clang loop(i2) unrollingandjam factor(4)
#pragma clang loop(j2) unrollingandjam factor(8)
#pragma clang loop(i1,j1,k1,i2,j2) interchange permutation(j1,k1,i1,j2,i2)
#pragma clang loop(i,j,k) tile sizes(64,2048,256) floor_ids(i1,j1,k1) tile_ids(i2,j2,k2) peel(rectangular)
#pragma clang loop id(i)
  for (int i = 0; i < M; i += 1)
    #pragma clang loop id(j)
    for (int j = 0; j < N; j += 1)
      #pragma clang loop id(k)
      for (int k = 0; k < K; k += 1)
        C[i][j] += A[i][k] * B[k][j];
}


#ifdef MAIN
#include <stdio.h>
#include <string.h>
int main() {
  double C[16][32];
  double A[16][64];
  double B[64][32];
  memset(C, 0, sizeof(C));
  memset(A, 0, sizeof(A));
  memset(B, 0, sizeof(B));
  A[1][2] = 2;
  A[1][3] = 3;
  B[2][4] = 5;
  B[3][4] = 7;
  matmul(16,32,64,C,A,B);
  printf("(%0.0f)\n", C[1][4]); // C[1][4] = A[1][2]*B[2][4] + A[1][3]*B[3][4] = 2*5 + 3*7 = 10 + 21 = 31
  return 0;
}
#endif


// PRINT-LABEL: void matmul(int M, int N, int K, double C[const restrict static M][N], double A[const restrict static M][K], double B[const restrict static K][N]) __attribute__((noinline)) {
// PRINT-NEXT: #pragma clang loop(i1) pack array(B) allocate(malloc) isl_redirect("{ Prefix[c,j,k]   -> [MyB[x,y] -> MyPacked_B[floord(y,8) mod 256,x mod 256,y mod 8]] }")
// PRINT-NEXT: #pragma clang loop(j2) pack array(A) allocate(malloc) isl_redirect("{ Prefix[c,j,k,l] -> [MyA[x,y] -> MyPacked_A[floord(x,4) mod 16,y mod 256,x mod 4]] }")
// PRINT-NEXT: #pragma clang loop(i2) unrollingandjam factor(4)
// PRINT-NEXT: #pragma clang loop(j2) unrollingandjam factor(8)
// PRINT-NEXT: #pragma clang loop(i1, j1, k1, i2, j2) interchange permutation(j1, k1, i1, j2, i2)
// PRINT-NEXT: #pragma clang loop(i, j, k) tile sizes(64, 2048, 256) floor_ids(i1, j1, k1) tile_ids(i2, j2, k2) peel(rectangular)
// PRINT-NEXT: #pragma clang loop id(i)
// PRINT-NEXT:     for (int i = 0; i < M; i += 1)
// PRINT-NEXT: #pragma clang loop id(j)
// PRINT-NEXT:         for (int j = 0; j < N; j += 1)
// PRINT-NEXT: #pragma clang loop id(k)
// PRINT-NEXT:             for (int k = 0; k < K; k += 1)
// PRINT-NEXT:                 C[i][j] += A[i][k] * B[k][j];
// PRINT-NEXT: }


// IR-LABEL: define dso_local void @matmul(i32 %M, i32 %N, i32 %K, double* noalias nonnull align 8 %C, double* noalias nonnull align 8 %A, double* noalias nonnull align 8 %B) #0 {
// IR-DAG:   !"llvm.loop.tile.enable"
// IR-DAG:   !"llvm.loop.interchange.enable"
// IR-DAG:   !"llvm.data.pack.enable"
// IR-DAG:   !"llvm.loop.unroll_and_jam.enable"


// AST: {
// AST: 	if (M >= 64) {
// AST: 		// Loop: interchange
// AST: 		for (int c0 = 0; c0 < floord(N, 2048); c0 += 1) {
// AST: 			// Loop: interchange
// AST: 			for (int c1 = 0; c1 < floord(K, 256); c1 += 1) {
// AST: 				for (int c5 = 0; c5 <= 255; c5 += 1)
// AST: 					for (int c6 = 0; c6 <= 255; c6 += 1)
// AST: 						for (int c7 = 0; c7 <= 7; c7 += 1)
// AST: 							CopyStmt_1(0, c0, c1, c5, c6, c7);
// AST: 				// Loop: i1
// AST: 				for (int c2 = 0; c2 < floord(M, 64); c2 += 1) {
// AST: 					for (int c7 = 0; c7 <= 15; c7 += 1)
// AST: 						for (int c8 = 0; c8 <= 255; c8 += 1)
// AST: 							for (int c9 = 0; c9 <= 3; c9 += 1)
// AST: 								CopyStmt_0(0, c0, c1, c2, c7, c8, c9);
// AST: 					// Loop: unrolled-and-jam
// AST: 					for (int c3 = 0; c3 <= 2047; c3 += 8) {
// AST: 						// Loop: unrolled-and-jam
// AST: 						for (int c4 = 0; c4 <= 63; c4 += 4) {
// AST: 							// Loop: jammed
// AST: 							for (int c5 = 0; c5 <= 255; c5 += 1) {
// AST: 								Stmt_for_body8(64 * c2 + c4, 2048 * c0 + c3, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 1, 2048 * c0 + c3, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 2, 2048 * c0 + c3, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 3, 2048 * c0 + c3, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4, 2048 * c0 + c3 + 1, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 1, 2048 * c0 + c3 + 1, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 2, 2048 * c0 + c3 + 1, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 3, 2048 * c0 + c3 + 1, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4, 2048 * c0 + c3 + 2, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 1, 2048 * c0 + c3 + 2, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 2, 2048 * c0 + c3 + 2, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 3, 2048 * c0 + c3 + 2, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4, 2048 * c0 + c3 + 3, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 1, 2048 * c0 + c3 + 3, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 2, 2048 * c0 + c3 + 3, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 3, 2048 * c0 + c3 + 3, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4, 2048 * c0 + c3 + 4, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 1, 2048 * c0 + c3 + 4, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 2, 2048 * c0 + c3 + 4, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 3, 2048 * c0 + c3 + 4, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4, 2048 * c0 + c3 + 5, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 1, 2048 * c0 + c3 + 5, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 2, 2048 * c0 + c3 + 5, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 3, 2048 * c0 + c3 + 5, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4, 2048 * c0 + c3 + 6, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 1, 2048 * c0 + c3 + 6, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 2, 2048 * c0 + c3 + 6, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 3, 2048 * c0 + c3 + 6, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4, 2048 * c0 + c3 + 7, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 1, 2048 * c0 + c3 + 7, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 2, 2048 * c0 + c3 + 7, 256 * c1 + c5);
// AST: 								Stmt_for_body8(64 * c2 + c4 + 3, 2048 * c0 + c3 + 7, 256 * c1 + c5);
// AST: 							}
// AST: 						}
// AST: 					}
// AST: 				}
// AST: 			}
// AST: 		}
// AST: 	}
// AST: 	if (K >= 1 && N % 2048 == 0) {
// AST: 		for (int c0 = 0; c0 < floord(M, 64); c0 += 1)
// AST: 			for (int c1 = 0; c1 < N / 2048; c1 += 1)
// AST: 				for (int c3 = 0; c3 <= 63; c3 += 1)
// AST: 					for (int c4 = 0; c4 <= 2047; c4 += 1)
// AST: 						for (int c5 = 0; c5 < K % 256; c5 += 1)
// AST: 							Stmt_for_body8_clone(64 * c0 + c3, 2048 * c1 + c4, -(K % 256) + K + c5);
// AST: 	} else if (N >= 1 && K >= 1 && N % 2048 >= 1) {
// AST: 		for (int c0 = 0; c0 < floord(M, 64); c0 += 1) {
// AST: 			for (int c1 = 0; c1 < floord(N, 2048); c1 += 1)
// AST: 				for (int c3 = 0; c3 <= 63; c3 += 1)
// AST: 					for (int c4 = 0; c4 <= 2047; c4 += 1)
// AST: 						for (int c5 = 0; c5 < K % 256; c5 += 1)
// AST: 							Stmt_for_body8_clone(64 * c0 + c3, 2048 * c1 + c4, -(K % 256) + K + c5);
// AST: 			for (int c2 = 0; c2 <= floord(K - 1, 256); c2 += 1)
// AST: 				for (int c3 = 0; c3 <= 63; c3 += 1)
// AST: 					for (int c4 = 0; c4 <= 2046; c4 += 1)
// AST: 						if (N % 2048 >= c4 + 1)
// AST: 							for (int c5 = 0; c5 <= min(255, K - 256 * c2 - 1); c5 += 1)
// AST: 								Stmt_for_body8_clone(64 * c0 + c3, -(N % 2048) + N + c4, 256 * c2 + c5);
// AST: 		}
// AST: 	}
// AST: 	if (M >= 1 && M % 64 >= 1)
// AST: 		for (int c1 = 0; c1 <= floord(N - 1, 2048); c1 += 1)
// AST: 			for (int c2 = 0; c2 <= floord(K - 1, 256); c2 += 1)
// AST: 				for (int c3 = 0; c3 <= 62; c3 += 1)
// AST: 					if (M % 64 >= c3 + 1)
// AST: 						for (int c4 = 0; c4 <= min(2047, N - 2048 * c1 - 1); c4 += 1)
// AST: 							for (int c5 = 0; c5 <= min(255, K - 256 * c2 - 1); c5 += 1)
// AST: 								Stmt_for_body8_clone(-(M % 64) + M + c3, 2048 * c1 + c4, 256 * c2 + c5);
// AST: }
// AST: else
// AST: 	{  /* original code */ }


// TRANS: %malloccall = tail call dereferenceable_or_null(131072) i8* @malloc(i64 131072)
// TRANS: %malloccall60 = tail call dereferenceable_or_null(4194304) i8* @malloc(i64 4194304) #3
// TRANS: tail call void @free(i8* %malloccall)
// TRANS: tail call void @free(i8* %malloccall60)
// TRANS-DAG: Packed_MemRef_A
// TRANS-DAG: Packed_MemRef_B
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add
// TRANS: store double %p_add


// RESULT: (31)
