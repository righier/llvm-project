// RUN: %clang_cc1 -flegacy-pass-manager -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -fno-unroll-loops -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-reschedule=0 -mllvm -polly-pattern-matching-based-opts=0 -mllvm -polly-use-llvm-names -emit-llvm -o /dev/null %s -mllvm -debug-only=polly-ast 2>&1 > /dev/null | FileCheck --check-prefix=AST %s
// RUN: %clang_cc1 -flegacy-pass-manager -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -fno-unroll-loops -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-reschedule=0 -mllvm -polly-pattern-matching-based-opts=0 -mllvm -polly-use-llvm-names -emit-llvm -o - %s | FileCheck --check-prefix=TRANS %s

__attribute__((noinline))
void loopnest(int M, int N, double C[const restrict static M][N]) {
  for (int i = 0; i < M; i += 1)
    for (int j = 0; j < N; j += 1)
        C[i][j] = i + j;
}


// AST:      // anon loop
// AST-NEXT: for (int c0 = 0; c0 < M; c0 += 1) {
// AST-NEXT:   // anon loop
// AST-NEXT:   for (int c1 = 0; c1 < N; c1 += 1) 
// AST-NEXT:     Stmt_for_body4(c0, c1);
// AST-NEXT: }


// TRANS-NOT: GOMP
// TRANS-NOT: kmp
