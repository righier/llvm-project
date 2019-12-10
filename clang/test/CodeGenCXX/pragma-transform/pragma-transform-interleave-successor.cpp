// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++11 -fexperimental-transform-pragma -emit-llvm -o - %s | FileCheck %s

extern "C" void pragma_transform_interleave_successor(int *List, int Length) {
#pragma clang transform unroll
#pragma clang transform interleave
  for (int i = 0; i < Length; i++) {
// CHECK: br label %{{.*}}, !llvm.loop ![[LOOP:[0-9]+]]
    List[i] = i * 2;
  }
}


// CHECK-DAG: ![[LOOP]] = distinct !{![[LOOP]], ![[INTERLEAVE_ENABLE:[0-9]+]], ![[VECTORIZE_DISABLE:[0-9]+]], ![[VECTORIZE_FOLLOWUP_ALL:[0-9]+]], ![[VECTORIZE_FOLLOWUP_VECTORIZED:[0-9]+]], ![[DISABLE_NONFORCED:[0-9]+]]}
// CHECK-DAG: ![[INTERLEAVE_ENABLE]] = !{!"llvm.loop.vectorize.enable", i1 true}
// CHECK-DAG: ![[VECTORIZE_DISABLE]] = !{!"llvm.loop.vectorize.width", i32 1}
// CHECK-DAG: ![[VECTORIZE_FOLLOWUP_ALL]] = !{!"llvm.loop.vectorize.followup_all", ![[LOOP_VECTORIZE_ALL:[0-9]+]]}
// CHECK-DAG: ![[VECTORIZE_FOLLOWUP_VECTORIZED]] = !{!"llvm.loop.vectorize.followup_vectorized", ![[LOOP_VECTORIZED:[0-9]+]]}
// CHECK-DAG: ![[DISABLE_NONFORCED]] = !{!"llvm.loop.disable_nonforced"}

// CHECK-DAG: ![[LOOP_VECTORIZE_ALL]] = distinct !{![[LOOP_VECTORIZE_ALL]], ![[DISABLE_NONFORCED:[0-9]+]]}

// CHECK-DAG: ![[LOOP_VECTORIZED]] = distinct !{![[LOOP_VECTORIZED]],  ![[UNROLL_ENABLE:[0-9]+]], ![[UNROLL_FOLLOWUP_ALL:[0-9]+]]}
// CHECK-DAG: ![[UNROLL_ENABLE]] = !{!"llvm.loop.unroll.enable"}
// CHECK-DAG: ![[UNROLL_FOLLOWUP_ALL:[0-9]+]] = !{!"llvm.loop.unroll.followup_all", !12}

// CHECK-DAG: ![[LOOP_UNROLLED:[0-9]+]] = distinct !{![[LOOP_UNROLLED:[0-9]+]], ![[DISABLE_NONFORCED:[0-9]+]]}
