// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++11 -fexperimental-transform-pragma -emit-llvm -o - %s | FileCheck %s

extern "C" void pragma_transform_unroll_partial(int *List, int Length) {
#pragma clang transform unroll partial(8)
  for (int i = 0; i < Length; i++) {
// CHECK: br label %{{.*}}, !llvm.loop ![[LOOP:[0-9]+]]
    List[i] = i * 2;
  }
}


// CHECK-DAG: ![[LOOP]] = distinct !{![[LOOP]], ![[UNROLL_ENABLE:[0-9]+]], ![[UNROLL_FACTOR:[0-9]+]], ![[UNROLL_FOLLOWUP_ALL:[0-9]+]], ![[DISABLE_NONFORCED:[0-9]+]]}
// CHECK-DAG: ![[UNROLL_ENABLE]] = !{!"llvm.loop.unroll.enable"}
// CHECK-DAG: ![[UNROLL_FACTOR]] = !{!"llvm.loop.unroll.count", i32 8}
// CHECK-DAG: ![[UNROLL_FOLLOWUP_ALL]] = !{!"llvm.loop.unroll.followup_all", ![[LOOP_UNROLL_ALL:[0-9]+]]}
// CHECK-DAG: ![[DISABLE_NONFORCED]] = !{!"llvm.loop.disable_nonforced"}

// CHECK-DAG: ![[LOOP_UNROLL_ALL]] = distinct !{![[LOOP_UNROLL_ALL]], ![[DISABLE_NONFORCED:[0-9]+]]}
