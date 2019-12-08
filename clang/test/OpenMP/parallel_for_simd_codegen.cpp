// RUN: %clang_cc1 -verify -fopenmp -x c++ -triple x86_64-unknown-unknown -emit-llvm %s -fexceptions -fcxx-exceptions -o - | FileCheck %s --check-prefix=OMP45 --check-prefix=CHECK
// RUN: %clang_cc1 -fopenmp -x c++ -std=c++11 -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -emit-pch -o %t %s
// RUN: %clang_cc1 -fopenmp -x c++ -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -debug-info-kind=limited -std=c++11 -include-pch %t -verify %s -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 -verify -triple x86_64-apple-darwin10 -fopenmp -fexceptions -fcxx-exceptions -debug-info-kind=line-tables-only -x c++ -emit-llvm %s -o - | FileCheck %s --check-prefix=TERM_DEBUG

// RUN: %clang_cc1 -verify -fopenmp -fopenmp-version=50 -x c++ -triple x86_64-unknown-unknown -emit-llvm %s -fexceptions -fcxx-exceptions -o - | FileCheck %s --check-prefix=OMP50 --check-prefix=CHECK
// RUN: %clang_cc1 -fopenmp -fopenmp-version=50 -x c++ -std=c++11 -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -emit-pch -o %t %s
// RUN: %clang_cc1 -fopenmp -fopenmp-version=50 -x c++ -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -debug-info-kind=limited -std=c++11 -include-pch %t -verify %s -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 -verify -triple x86_64-apple-darwin10 -fopenmp -fopenmp-version=50 -fexceptions -fcxx-exceptions -debug-info-kind=line-tables-only -x c++ -emit-llvm %s -o - | FileCheck %s --check-prefix=TERM_DEBUG

// RUN: %clang_cc1 -verify -fopenmp-simd -x c++ -triple x86_64-unknown-unknown -emit-llvm %s -fexceptions -fcxx-exceptions -o - | FileCheck --check-prefix SIMD-ONLY0 %s
// RUN: %clang_cc1 -fopenmp-simd -x c++ -std=c++11 -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -emit-pch -o %t %s
// RUN: %clang_cc1 -fopenmp-simd -x c++ -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -debug-info-kind=limited -std=c++11 -include-pch %t -verify %s -emit-llvm -o - | FileCheck --check-prefix SIMD-ONLY0 %s
// RUN: %clang_cc1 -verify -triple x86_64-apple-darwin10 -fopenmp-simd -fexceptions -fcxx-exceptions -debug-info-kind=line-tables-only -x c++ -emit-llvm %s -o - | FileCheck --check-prefix SIMD-ONLY0 %s

// RUN: %clang_cc1 -verify -fopenmp-simd -fopenmp-version=50  -x c++ -triple x86_64-unknown-unknown -emit-llvm %s -fexceptions -fcxx-exceptions -o - | FileCheck --check-prefix SIMD-ONLY0 %s
// RUN: %clang_cc1 -fopenmp-simd -fopenmp-version=50  -x c++ -std=c++11 -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -emit-pch -o %t %s
// RUN: %clang_cc1 -fopenmp-simd -fopenmp-version=50  -x c++ -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -debug-info-kind=limited -std=c++11 -include-pch %t -verify %s -emit-llvm -o - | FileCheck --check-prefix SIMD-ONLY0 %s
// RUN: %clang_cc1 -verify -triple x86_64-apple-darwin10 -fopenmp-simd -fopenmp-version=50  -fexceptions -fcxx-exceptions -debug-info-kind=line-tables-only -x c++ -emit-llvm %s -o - | FileCheck --check-prefix SIMD-ONLY0 %s
// expected-no-diagnostics
#ifndef HEADER
#define HEADER



// CHECK-LABEL: if_clause
void if_clause(int a) {
  #pragma omp parallel for simd if(a) schedule(static, 1)
for (int i = 0; i < 10; ++i);
}
// CHECK: call void @__kmpc_for_static_init_4(
// OMP50: [[COND:%.+]] = trunc i8 %{{.+}} to i1
// OMP50: br i1 [[COND]], label {{%?}}[[THEN:.+]], label {{%?}}[[ELSE:.+]]

// OMP50: [[THEN]]:
// OMP45: br label {{.+}}, !llvm.loop ![[VECT:.+]]
// OMP50: br label {{.+}}, !llvm.loop ![[VECT:.+]]
// OMP50: [[ELSE]]:
// OMP50: br label {{.+}}, !llvm.loop ![[NOVECT:.+]]
// CHECK: call void @__kmpc_for_static_fini(

// OMP45: call void @__kmpc_for_static_init_8(%struct.ident_t* {{[^,]+}}, i32 %{{[^,]+}}, i32 34, i32* %{{[^,]+}}, i64* [[LB:%[^,]+]], i64* [[UB:%[^,]+]], i64* [[STRIDE:%[^,]+]], i64 1, i64 1)
// OMP45: [[UB_VAL:%.+]] = load i64, i64* [[UB]],
// OMP45: [[CMP:%.+]] = icmp sgt i64 [[UB_VAL]], 15
// OMP45: br i1 [[CMP]], label %[[TRUE:.+]], label %[[FALSE:[^,]+]]
// OMP45: [[TRUE]]:
// OMP45: br label %[[SWITCH:[^,]+]]
// OMP45: [[FALSE]]:
// OMP45: [[UB_VAL:%.+]] = load i64, i64* [[UB]],
// OMP45: br label %[[SWITCH]]
// OMP45: [[SWITCH]]:
// OMP45: [[UP:%.+]] = phi i64 [ 15, %[[TRUE]] ], [ [[UB_VAL]], %[[FALSE]] ]
// OMP45: store i64 [[UP]], i64* [[UB]],
// OMP45: [[LB_VAL:%.+]] = load i64, i64* [[LB]],
// OMP45: store i64 [[LB_VAL]], i64* [[T1_OMP_IV:%[^,]+]],

// ...
// OMP45: [[IV:%.+]] = load i64, i64* [[T1_OMP_IV]]
// OMP45-NEXT: [[UB_VAL:%.+]] = load i64, i64* [[UB]]
// OMP45-NEXT: [[CMP1:%.+]] = icmp sle i64 [[IV]], [[UB_VAL]]
// OMP45-NEXT: br i1 [[CMP1]], label %[[T1_BODY:.+]], label %[[T1_END:[^,]+]]
// OMP45: [[T1_BODY]]:
// Loop counters i and j updates:
// OMP45: [[IV1:%.+]] = load i64, i64* [[T1_OMP_IV]]
// OMP45-NEXT: [[I_1:%.+]] = sdiv i64 [[IV1]], 4
// OMP45-NEXT: [[I_1_MUL1:%.+]] = mul nsw i64 [[I_1]], 1
// OMP45-NEXT: [[I_1_ADD0:%.+]] = add nsw i64 0, [[I_1_MUL1]]
// OMP45-NEXT: [[I_2:%.+]] = trunc i64 [[I_1_ADD0]] to i32
// OMP45-NEXT: store i32 [[I_2]], i32*
// OMP45: [[IV2:%.+]] = load i64, i64* [[T1_OMP_IV]]
// OMP45: [[IV2_1:%.+]] = load i64, i64* [[T1_OMP_IV]]
// OMP45-NEXT: [[DIV_1:%.+]] = sdiv i64 [[IV2_1]], 4
// OMP45-NEXT: [[MUL_1:%.+]] = mul nsw i64 [[DIV_1]], 4
// OMP45-NEXT: [[J_1:%.+]] = sub nsw i64 [[IV2]], [[MUL_1]]
// OMP45-NEXT: [[J_2:%.+]] = mul nsw i64 [[J_1]], 2
// OMP45-NEXT: [[J_2_ADD0:%.+]] = add nsw i64 0, [[J_2]]
// OMP45-NEXT: store i64 [[J_2_ADD0]], i64*
// simd.for.inc:
// OMP45: [[IV3:%.+]] = load i64, i64* [[T1_OMP_IV]]
// OMP45-NEXT: [[INC:%.+]] = add nsw i64 [[IV3]], 1
// OMP45-NEXT: store i64 [[INC]], i64*
// OMP45-NEXT: br label {{%.+}}
// OMP45: [[T1_END]]:
// OMP45: call void @__kmpc_for_static_fini(%struct.ident_t* {{.+}}, i32 %{{.+}})
// OMP45: ret void
//

// OMP45-NOT: !{!"llvm.loop.vectorize.enable", i1 false}
// OMP45-DAG: ![[VECT]] = distinct !{![[VECT]], ![[VM:.+]]}
// OMP45-DAG: ![[VM]] = !{!"llvm.loop.vectorize.enable", i1 true}
// OMP45-NOT: !{!"llvm.loop.vectorize.enable", i1 false}
// OMP50-DAG: ![[VECT]] = distinct !{![[VECT]], ![[VM:.+]]}
// OMP50-DAG: ![[VM]] = !{!"llvm.loop.vectorize.enable", i1 true}
// OMP50-DAG: ![[NOVECT]] = distinct !{![[NOVECT]], ![[NOVM:.+]]}
// OMP50-DAG: ![[NOVM]] = !{!"llvm.loop.vectorize.enable", i1 false}

#endif // HEADER

