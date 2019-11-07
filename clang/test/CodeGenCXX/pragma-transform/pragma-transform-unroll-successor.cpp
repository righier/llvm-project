// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++11 -fexperimental-transform-pragma -emit-llvm -o - %s | FileCheck %s

extern "C" void pragma_transform_unroll_successor(int *List, int Length) {
#pragma clang transform distribute
#pragma clang transform unroll
  for (int i = 0; i < Length; i++) {
// CHECK: br label %{{.*}}, !llvm.loop ![[LOOP:[0-9]+]]
    List[i] = i * 2;
  }
}


// CHECK-DAG: ![[LOOP]] = distinct !{![[LOOP]], ![[UNROLL_ENABLE:[0-9]+]], ![[UNROLL_FOLLOWUP_ALL:[0-9]+]], ![[UNROLL_FOLLOWUP_UNROLLED:[0-9]+]], ![[DISABLE_NONFORCED:[0-9]+]]}
// CHECK-DAG: ![[UNROLL_ENABLE]] = !{!"llvm.loop.unroll.enable"}
// CHECK-DAG: ![[UNROLL_FOLLOWUP_ALL]] = !{!"llvm.loop.unroll.followup_all", ![[LOOP_UNROLL_ALL:[0-9]+]]}
// CHECK-DAG: ![[UNROLL_FOLLOWUP_UNROLLED]] = !{!"llvm.loop.unroll.followup_unrolled", ![[LOOP_UNROLLED:[0-9]+]]}

// CHECK-DAG: ![[LOOP_UNROLLED]] = distinct !{![[LOOP_UNROLLED]], ![[DISTRIBUTE_ENABLE:[0-9]+]], ![[DISTRIBUTE_FOLLOWUP_ALL:[0-9]+]]}
// CHECK-DAG: ![[DISTRIBUTE_ENABLE]] = !{!"llvm.loop.distribute.enable", i1 true}
// CHECK-DAG: ![[DISTRIBUTE_FOLLOWUP_ALL]] = !{!"llvm.loop.distribute.followup_all", ![[LOOP_DISTRIBUTE_ALL:[0-9]+]]}

// CHECK-DAG: ![[LOOP_UNROLL_ALL]] = distinct !{![[LOOP_UNROLL_ALL]], ![[DISABLE_NONFORCED:[0-9]+]]}
// CHECK-DAG: ![[LOOP_DISTRIBUTE_ALL]] = distinct !{![[LOOP_DISTRIBUTE_ALL]], ![[DISABLE_NONFORCED:[0-9]+]]}
// CHECK-DAG: ![[DISABLE_NONFORCED]] = !{!"llvm.loop.disable_nonforced"}
