// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++11 -fexperimental-transform-pragma -emit-llvm -o - %s | FileCheck %s

extern "C" void pragma_transform_unrollandjam_successor(int *List, int Length) {
#pragma clang transform distribute
#pragma clang transform unrollandjam partial(4)
  for (int i = 0; i < Length; i++) {
    for (int j = 0; j < Length; j++)  {
      List[j] += i * 2;
// CHECK: br label %{{.*}}, !llvm.loop ![[LOOP_INNER:[0-9]+]]
    }
// CHECK: br label %{{.*}}, !llvm.loop ![[LOOP_OUTER:[0-9]+]]
  }
}


// CHECK-DAG: ![[LOOP_INNER]] = distinct !{![[LOOP_INNER]], ![[DISABLE_NONFORCED:[0-9]+]]}
// CHECK-DAG: ![[DISABLE_NONFORCED]] = !{!"llvm.loop.disable_nonforced"}

// CHECK-DAG: ![[LOOP_OUTER]] = distinct !{![[LOOP_OUTER]], ![[UNROLLANDJAM_ENABLE:[0-9]+]], ![[UNROLLANDJAM_COUNT:[0-9]+]], ![[UNROLLANDJAM_FOLLOWUP_ALL:[0-9]+]], ![[UNROLLANDJAM_FOLLOWUP_OUTER:[0-9]+]], ![[DISABLE_NONFORCED]]}
// CHECK-DAG: ![[UNROLLANDJAM_ENABLE]] = !{!"llvm.loop.unroll_and_jam.enable"}
// CHECK-DAG: ![[UNROLLANDJAM_COUNT]] = !{!"llvm.loop.unroll_and_jam.count", i32 4}
// CHECK-DAG: ![[UNROLLANDJAM_FOLLOWUP_ALL]] = !{!"llvm.loop.unroll_and_jam.followup_all", ![[LOOP_UNROLLANDJAM_ALL:[0-9]+]]}
// CHECK-DAG: ![[UNROLLANDJAM_FOLLOWUP_OUTER]] = !{!"llvm.loop.unroll_and_jam.followup_outer", ![[LOOP_UNROLLANDJAM_OUTER:[0-9]+]]}

// CHECK-DAG: ![[LOOP_UNROLLANDJAM_ALL]] = distinct !{![[LOOP_UNROLLANDJAM_ALL]], ![[DISABLE_NONFORCED]]}

// CHECK-DAG: ![[LOOP_UNROLLANDJAM_OUTER]] = distinct !{![[LOOP_UNROLLANDJAM_OUTER]], ![[DISTRIBUTE_ENABLE:[0-9]+]], ![[DISTRIBUTE_FOLLOWUP_ALL:[0-9]+]]}
// CHECK-DAG: ![[DISTRIBUTE_ENABLE]] = !{!"llvm.loop.distribute.enable", i1 true}
// CHECK-DAG: ![[DISTRIBUTE_FOLLOWUP_ALL]] = !{!"llvm.loop.distribute.followup_all", ![[LOOP_DISTRIBUTE_FOLLOWUP_ALL:[0-9]+]]}

// CHECK-DAG: ![[LOOP_DISTRIBUTE_FOLLOWUP_ALL]] = distinct !{![[LOOP_DISTRIBUTE_FOLLOWUP_ALL]], ![[DISABLE_NONFORCED]]}
