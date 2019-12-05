// RUN: %clang_cc1 -verify -fopenmp -x c++ -triple x86_64-unknown-unknown -emit-llvm %s -fexceptions -fcxx-exceptions -o - | FileCheck %s
// RUN: %clang_cc1 -fopenmp -x c++ -std=c++11 -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -emit-pch -o %t %s
// RUN: %clang_cc1 -fopenmp -x c++ -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -debug-info-kind=limited -std=c++11 -include-pch %t -verify %s -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 -verify -triple x86_64-apple-darwin10 -fopenmp -fexceptions -fcxx-exceptions -debug-info-kind=line-tables-only -x c++ -emit-llvm %s -o - | FileCheck %s --check-prefix=TERM_DEBUG
// RUN: %clang_cc1 -verify -fopenmp -x c++ -triple x86_64-unknown-unknown -emit-llvm %s -fexceptions -fcxx-exceptions -o - -fopenmp-version=50 -DOMP5 | FileCheck %s --check-prefix=CHECK --check-prefix=OMP50 --dump-input=fail -vv
// RUN: %clang_cc1 -fopenmp -x c++ -std=c++11 -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -emit-pch -o %t %s -fopenmp-version=50 -DOMP5
// RUN: %clang_cc1 -fopenmp -x c++ -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -debug-info-kind=limited -std=c++11 -include-pch %t -verify %s -emit-llvm -o - -fopenmp-version=50 -DOMP5 | FileCheck %s --check-prefix=CHECK --check-prefix=OMP50

// RUN: %clang_cc1 -verify -fopenmp-simd -x c++ -triple x86_64-unknown-unknown -emit-llvm %s -fexceptions -fcxx-exceptions -o - | FileCheck %s
// RUN: %clang_cc1 -fopenmp-simd -x c++ -std=c++11 -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -emit-pch -o %t %s
// RUN: %clang_cc1 -fopenmp-simd -x c++ -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -debug-info-kind=limited -std=c++11 -include-pch %t -verify %s -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 -verify -triple x86_64-apple-darwin10 -fopenmp-simd -fexceptions -fcxx-exceptions -debug-info-kind=line-tables-only -x c++ -emit-llvm %s -o - | FileCheck --check-prefix=TERM_DEBUG %s
// RUN: %clang_cc1 -verify -fopenmp-simd -x c++ -triple x86_64-unknown-unknown -emit-llvm %s -fexceptions -fcxx-exceptions -o - -fopenmp-version=50 -DOMP5 | FileCheck %s --check-prefix=CHECK --check-prefix=OMP50
// RUN: %clang_cc1 -fopenmp-simd -x c++ -std=c++11 -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -emit-pch -o %t %s -fopenmp-version=50 -DOMP5
// RUN: %clang_cc1 -fopenmp-simd -x c++ -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -debug-info-kind=limited -std=c++11 -include-pch %t -verify %s -emit-llvm -o - -fopenmp-version=50 -DOMP5 | FileCheck %s --check-prefix=CHECK --check-prefix=OMP50
// expected-no-diagnostics
 #ifndef HEADER
 #define HEADER



long long get_val() { return 0; }
double *g_ptr;

// CHECK-LABEL: define {{.*void}} @{{.*}}simple{{.*}}(float* {{.+}}, float* {{.+}}, float* {{.+}}, float* {{.+}})
void simple(float *a, float *b, float *c, float *d) {


  int A;
  // CHECK: store i32 -1, i32* [[A:%.+]],
  A = -1;

  int R;
  // CHECK: store i32 -1, i32* [[R:%[^,]+]],
  R = -1;
// CHECK: store i64 0, i64* [[OMP_IV8:%[^,]+]],
// CHECK: store i32 1, i32* [[R_PRIV:%[^,]+]],
#ifdef OMP5
  #pragma omp simd reduction(*:R) if(A)
#else
  #pragma omp simd reduction(*:R)
#endif
// OMP50:      [[A_VAL:%.+]] = load i32, i32* [[A]],
// OMP50-NEXT: [[COND:%.+]] = icmp ne i32 [[A_VAL]], 0
// OMP50-NEXT: br i1 [[COND]], label {{%?}}[[THEN:[^,]+]], label {{%?}}[[ELSE:[^,]+]]
// OMP50:      [[THEN]]:

// CHECK: br label %[[SIMD_LOOP8_COND:[^,]+]]
// CHECK: [[SIMD_LOOP8_COND]]:
// CHECK-NEXT: [[IV8:%.+]] = load i64, i64* [[OMP_IV8]]{{.*}}!llvm.access.group
// CHECK-NEXT: [[CMP8:%.+]] = icmp slt i64 [[IV8]], 7
// CHECK-NEXT: br i1 [[CMP8]], label %[[SIMPLE_LOOP8_BODY:.+]], label %[[SIMPLE_LOOP8_END:[^,]+]]
  for (long long i = -10; i < 10; i += 3) {
// CHECK: [[SIMPLE_LOOP8_BODY]]:
// Start of body: calculate i from IV:
// CHECK: [[IV8_0:%.+]] = load i64, i64* [[OMP_IV8]]{{.*}}!llvm.access.group
// CHECK-NEXT: [[LC_IT_1:%.+]] = mul nsw i64 [[IV8_0]], 3
// CHECK-NEXT: [[LC_IT_2:%.+]] = add nsw i64 -10, [[LC_IT_1]]
// CHECK-NEXT: store i64 [[LC_IT_2]], i64* [[LC:%[^,]+]],{{.+}}!llvm.access.group
// CHECK-NEXT: [[LC_VAL:%.+]] = load i64, i64* [[LC]]{{.+}}!llvm.access.group
// CHECK: store i32 %{{.+}}, i32* [[R_PRIV]],{{.+}}!llvm.access.group
    R *= i;
// CHECK: [[IV8_2:%.+]] = load i64, i64* [[OMP_IV8]]{{.*}}!llvm.access.group
// CHECK-NEXT: [[ADD8_2:%.+]] = add nsw i64 [[IV8_2]], 1
// CHECK-NEXT: store i64 [[ADD8_2]], i64* [[OMP_IV8]]{{.*}}!llvm.access.group
  }
// CHECK: [[SIMPLE_LOOP8_END]]:
// OMP50: br label {{%?}}[[EXIT:[^,]+]]
// OMP50: br label %[[SIMD_LOOP8_COND:[^,]+]]
// OMP50: [[SIMD_LOOP8_COND]]:
// OMP50-NEXT: [[IV8:%.+]] = load i64, i64* [[OMP_IV8]],{{[^!]*}}
// OMP50-NEXT: [[CMP8:%.+]] = icmp slt i64 [[IV8]], 7
// OMP50-NEXT: br i1 [[CMP8]], label %[[SIMPLE_LOOP8_BODY:.+]], label %[[SIMPLE_LOOP8_END:[^,]+]]
// OMP50: [[SIMPLE_LOOP8_BODY]]:
// Start of body: calculate i from IV:
// OMP50: [[IV8_0:%.+]] = load i64, i64* [[OMP_IV8]],{{[^!]*}}
// OMP50-NEXT: [[LC_IT_1:%.+]] = mul nsw i64 [[IV8_0]], 3
// OMP50-NEXT: [[LC_IT_2:%.+]] = add nsw i64 -10, [[LC_IT_1]]
// OMP50-NEXT: store i64 [[LC_IT_2]], i64* [[LC:%[^,]+]],{{[^!]*}}
// OMP50-NEXT: [[LC_VAL:%.+]] = load i64, i64* [[LC]],{{[^!]*}}
// OMP50: store i32 %{{.+}}, i32* [[R_PRIV]],{{[^!]*}}
// OMP50: [[IV8_2:%.+]] = load i64, i64* [[OMP_IV8]],{{[^!]*}}
// OMP50-NEXT: [[ADD8_2:%.+]] = add nsw i64 [[IV8_2]], 1
// OMP50-NEXT: store i64 [[ADD8_2]], i64* [[OMP_IV8]],{{[^!]*}}
// OMP50:      br label {{%?}}[[SIMD_LOOP8_COND]], {{.*}}!llvm.loop ![[DISABLE_VECT:.+]]
// OMP50: [[SIMPLE_LOOP8_END]]:
// OMP50: br label {{%?}}[[EXIT]]
// OMP50: [[EXIT]]:

// CHECK-DAG: [[R_VAL:%.+]] = load i32, i32* [[R]],
// CHECK-DAG: [[R_PRIV_VAL:%.+]] = load i32, i32* [[R_PRIV]],
// CHECK: [[RED:%.+]] = mul nsw i32 [[R_VAL]], [[R_PRIV_VAL]]
// CHECK-NEXT: store i32 [[RED]], i32* [[R]],
// CHECK-NEXT: ret void
}

#endif // HEADER

