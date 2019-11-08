
.. _PragmaClangLoop:

=================================================
User-Directed Code Transformations (Experimental)
=================================================

``#pragma clang transform``

.. contents::
   :local:
   :depth: 2


Introduction
============

Finding the code transformations that yields the fastest code is hard
--- or even impossible --- for the compiler the compiler to do,
especially when considering that the compiler has not all relevant
information at hand, for instance the number of loop iterations. Despite
the name, compiler optimizations almost never emits the optimal binary
(in the mathematical sense: there is no binary that runs faster) and only
conservatively applies high-level transformation to avoid make source code
slower that was already optimized by the programmer for a specific target.

User-directed transformations take the responsibility for the choice of
transformation (and optionally its legality) from the compiler to the
programmer. Compared the compiler, the programmer has additional means
of how to choose a sequence of transformations, such as deduction,
experience, experiments and autotuning.

Generally, a ``#pragma clang transform`` directive applies on the
for-, while- or do-loop that syntactically after it, or --- if the
directive precedes a another user-directed transformation --- the output
of the previous transformations. For instance, in

.. code-block:: c++

  #pragma clang transform transformation2
  #pragma clang transform transformation1
  for (int i = 0; i < N; i+=1)
    Stmt(i);

the for-loop is first transformed by ``transformation1``, and its result
is then transformed by ``transformation2``. Most transformations do not
allow additional control-flow out of the loop (such as ``break`` and
``return`` statements).


Comparison to writing source code in the transformed form
---------------------------------------------------------

Any user-directed transformation have the same effect as if the source
code had been written in the transformed form in the first place.
However, using directives has advantages:

 * Separation of semantics and performance-optimization

 * Less effort to experiment with transformations

 * Use the same code base with different optimization for different targets.
   For instance, a set of pragmas can be selected using the preprocessor

 * The compiler checks the transformation's effect on the programs's semantics.


Comparison to loop hints
------------------------

:ref:`#pragma clang loop <langref-loophint>` annotations differ in
syntax and semantics. Syntactically, ``#pragma clang loop`` annotations
are a list of attributes to a loop instead of a directive (i.e. a
transformation). Semantically, the loop hint options are unordered.
That is, every type of transformation is applied at most once and its
application order fixed. Moreover, it is possible that a pass in the
optimization pipeline invalidates a transformation. For instance, a loop
may be fully unrolled before the LoopVectorize pass can apply the
``vectorize_width(4)`` hint.


Comparison to OpenMP
--------------------

The syntax of ``#pragma clang transform`` is intentionally similar
to :doc:`OpenMP <OpenMPSupport>`'s directive + clauses syntax.
In contrast to typical compiler hints (such as :ref:`langref-loophint`),
the OpenMP directives such as ``simd`` are applied without a legality
check by default. That is, it is the programmer's responsibility the
ensure that the transformation preserves the program's semantics.
Clauses such as ``safelen`` may alter this behavior, but generally it is
incorrect if the compiler does not apply the transformation.

OpenMP has the advantage of being a standard supported by multiple compilers.
Some compilers support additional user-directives, including Clang's
loop hints and transformations, but may differ in semantics and requires
the preprocessor to select the correct directive for each compiler.

There is an effort to add more loop transformations into the OpenMP specification which would then be supported by all OpenMP-capable compilers.
We intend to reuse the infrastructure for ``#pragma clang transform``
for these transformations, but semantic differences will remain: OpenMP
directives do not require the compiler to verify the legality of a
transformation while loop hints by default will ensure that the program's
semantics does not change.


Representation in LLVM-IR
-------------------------

The directives are emitted as `Loop metadata
<http://llvm.org/docs/LangRef.html#llvm-loop>`__, the same mechanism
also used by :ref:`loop hints <langref-loophint>`. The largest
difference is that `#pragma clang transform` directives emit
followup-attributes consistent with the order of pragmas in the source
code.

See the `LLVM documentation on Transformation Metadata
<https://llvm.org/docs/TransformMetadata.html#transformation-metadata>`__
for details on loop metadata.


Current Limitations
-------------------

The current implementation is experimental an requires the
``-Xclang -fexperimental-transform-pragma`` command line flag to be enabled.
Otherwise, it is handle like an unknown pragma
(including a warning with ``-Wall``). In addition, the following
restrictions apply:

 - LLVM's default pass pipeline (new and legacy) only includes passes
   for the transformations: LoopFullUnroll, LoopVectorize (also applies
   interleaving), LoopUnrollAndJam and LoopUnroll (for partial unrolling).
   It is only able to apply transformations in this order.

 - An extension to `Polly <http://polly.llvm.org>`__ is able to apply all
   most transformations. But is not enabled by default. If Polly was
   enabled during the build of Clang, it can be enabled using
   ``-mllvm -polly``.

 - The user-directed transformations are passed to the optimization via
   loop metadata. The semantics of loop metadata allow, or even require
   them to be `dropped
   <http://llvm.org/docs/LangRef.html#metadata-nodes-and-metadata-strings>`__.
   Therefore it is possible that transformations are silently ignored
   without warning or :ref:`optimization remarks <rpass>`.

 - It is not possible to apply `#pragma clang transform` with
   ``#pragma clang loop`` or OpenMP pragmas on the same loops.
   However, it is possible to, for instance, apply user-directed
   transformations on loops within the body of an OpenMP parallel loop.


Future Plans
------------

 - More transformations: Tiling, interchange, peeling,
   skewing/wavefronting, collapse, split, concatenate, fusion,
   distribute/fission, array packing, space-filling curves, prefetching,
   reversal, blocking, etc.

 - In addition of applying the transformation on the loop of the next line,
   it can be applied to a loop identified by name. Loops are tagged with
   identifiers with an ``id`` directive or assigned to output loops by
   the transformation.

   This can be used to apply transformations on output loops that are
   not the first outermost loop of the preceding transformation.
   For instance, to apply vectorization to the innermost loop after tiling.
   Writing ``#pragma clang transform vectorize`` before
   ``#pragma clang transform tile`` would try vectorize the outermost
   loop.

 - Add a `check_safety` clause to all transformations that may change
   the code's semantics. It can be used to override the compiler's
   legality check and/or runtime fallback.

 - Improve the new pass manager pipeline to re-run all passes that
   process user directives in a loop until all are processed.

 - Use the OpenMPIRBuilder to apply transformations that do not require
   a legality check.

 - In the long term, use a dedicated representation of loop nests and
   apply transformations on this representation.

 - Allow combinations with OpenMP directives.


Supported Transformations
=========================

.. list-table:: Directives Overview
   :widths: auto

   * - Directive
     - Input
     - Semantic safety check
     - Transformable output?
     - Remainder

   * - ``unroll``
     - 1 loop
     - unconditional
     - no
     - epilogue

   * - ``unroll full``
     - 1 loop
     - unconditional
     - no
     - *n/a*

   * - ``unroll partial(n)``
     - 1 loop
     - unconditional
     - yes, unrolled
     - epilogue

   * - ``unrollandjam``
     - 2 directly nested loops
     - compile-time
     - no
     - epilogue

   * - ``unrollandjam partial(n)``
     - 2 directly nested loops
     - compile-time
     - yes, outer unrolled
     - epilogue

   * - ``vectorize``
     - 1 innermost loop
     - compile-time and fallback at run-time
     - yes, vectorized
     - epilogue

   * - ``inerleave``
     - 1 innermost loop
     - compile-time and fallback at run-time
     - yes, interleaved
     - epilogue


Loop Unrolling
--------------

Unrolling comes in two variants: Partial unrolling, which reduces the
loop trip count, and full unrolling, which completely removes the loop.
When neither of the ``partial`` or ``full`` clauses are specified,
the compiler is free to choose the variant, including the unroll-factor itself.
Clang may also chose to not unroll the loop in this case.

The corresponding is loop hint is :ref:`langext-unroll`.

The generated loop metadata is described in the `LLVM documentation
<https://llvm.org/docs/TransformMetadata.html#loop-unrolling>`__ and
`reference <https://llvm.org/docs/LangRef.html#llvm-loop-unroll>`__.


``full`` clause
^^^^^^^^^^^^^^^^

The full clause completely unrolls the loop. The loop's trip count must
be known at compile-time. Since the loop is removed, applying a
transformation after full unrolling is an error.

As an example, the code equivalent to

.. code-block:: c++

  #pragma clang unroll full
  for (int i = 0; i < 4; i+=1)
    Stmt(i);


is approximately

.. code-block:: c++

  {
    Stmt(0);
    Stmt(1);
    Stmt(2);
    Stmt(3);
  }


``partial(n)`` clause
^^^^^^^^^^^^^^^^^^^^^^

Partial unrolling can be thought of strip-mining followed by a full
unroll of the inner loop. It can also applied when the trip count is
unknown at compile-time (*runtime unrolling*). The unroll-factor
:math:`n` must be a compile-time constant. An epilogue/remainder can be
necessary to ensure all instances of original loop's body are executed.
Transformations applied to this directive are applied to the unrolled loop.

As an example, the following code

.. code-block:: c++

  #pragma clang unroll partial(2)
  for (int i = 0; i < N; i+=1)
    Stmt(i);


is approximately equivalent to

.. code-block:: c++

  for (int i = 0; i < N-1; i+=2) { // unrolled
    Stmt(i);
    Stmt(i+1);
  }
  if (N % 2)
    Stmt(N - 1); // remainder

Note that the unroll-factor can be smaller than the number of iterations
``N`` in which case only the remainder epilogue is executed.


Loop Unroll-And-Jam
-------------------

Unroll-and-jam executes multiple iterations in an outer loop, but
instead duplicating its body, interleaves the iterations of an inner loop.
The compiler only applies unroll-and-jam when it does not change the
code's semantics.

Currently, only partial unroll-and-jam is implemented. The jammed loop
must not contain any other loops and the unrolled loop must directly
surround the inner loop.

An epilogue/remainder can be necessary to ensure all instances of the
original loop's body are executed. Transformations applied to the
``unrollandjam`` directive are applied to the outer, unrolled loop.

The corresponding is loop hint is ``#pragma unroll_and_jam(n)``.
In contrast the loop hint, unroll and unroll-and-jam are considered two
independent transformations.

The generated loop metadata is described in the `LLVM documentation
<https://llvm.org/docs/TransformMetadata.html#llvm-loop-unroll-and-jam>`__ and
`reference <https://llvm.org/docs/LangRef.html#llvm-loop-unroll-and-jam>`__.


``partial`` clause
^^^^^^^^^^^^^^^^^^^

Specifies the unroll-factor. The unroll-factor :math:`n` must be a
compile-time constant. If the clause is not specified, the compiler heuristically determines the factor itself, which may include not
unrolling the loop at all.

For example, the code

.. code-block:: c++

  #pragma clang unrollandjam partial(2)
  for (int i = 0; i < M; i+=1)
    for (int j = 0; j < N; j+=1)
      Stmt(i,j);


is approximately equivalent to

.. code-block:: c++

  for (int i = 0; i < M; i+=2) { // unrolled
    for (int j = 0; j < N; j+=1) { // jammed
      Stmt(i,j);
      Stmt(i+1,j);
    }
  }
  if (M % 2) { // remainder
    for (int j = 0; j < N; j+=1)
      Stmt(M-1,j);
  }


Loop Vectorization
------------------

Loop vectorization replaces multiple loop iterations and replaces each
instruction with a vector instruction that processes multiple values,
one from each replaced iteration, at once
(Single Instruction Multiple Data, *SIMD*). It can be though of
unrolling followed by combining duplicated instructions into vector
instructions. This combining may alter the code's behavior, but the
compiler only applies vectorization if it does not.

Currently, vectorization is only implemented on innermost loops.
Additional restrictions apply, such that the input/output arrays must
not overlap.

An epilogue/remainder can be necessary to ensure that all instances of
the original loop's body are executed or to fallback to a serial version
when a runtime check cannot ensure that the vectorized version yields
the same result.

The corresponding loop hint is :ref:`langext-loopvectorize`, also described
`here <https://www.llvm.org/docs/Vectorizers.html#pragma-loop-hint-directives>`__.

The generated loop metadata is described in the `LLVM documentation
<https://llvm.org/docs/TransformMetadata.html#loop-vectorization-and-interleaving>`__
and `reference
<http://llvm.org/docs/LangRef.html#llvm-loop-vectorize-and-llvm-loop-interleave>`__.


`width(n)` clause
^^^^^^^^^^^^^^^^^^

Specifies the vector width. The parameter :math:`n` must be a
compile-time constant. If omitted, the compiler heuristically determines
a vector width itself, including the choice to not vectorize at all.

For example, the code

.. code-block:: c++

  #pragma clang vectorize width(4)
  for (int i = 0; i < N; i+=1)
      Stmt(i);


is approximately equivalent to

.. code-block:: c++

  int i = 0;
  for (; i < N-3; i+=4)
    Stmt(i..i+3);
  for (; i < N; i+=1)
    Stmt(i);

where ``Stmt(i..i+3)`` processes the 4 original iterations using vector
instructions.


Loop Interleaving
-----------------

Interleaving is similar to vectorization and unrolling. It can be either
described as vectorization without vector instructions or unrolling
where instead executing the entire original iterations sequentially in
the new loop body, the each instruction of the original loop body is
grouped ("*jammed*") together.

In contrast the :ref:`loop hints <langext-loopvectorize>`, vectorization
and interleaving are considered two distinct transformations.

Since LLVM's LoopVectorize pass also applies interleaving, it shares
that same `metadata documentation
<https://llvm.org/docs/TransformMetadata.html#loop-vectorization-and-interleaving>`__
and `reference <http://llvm.org/docs/LangRef.html#llvm-loop-vectorize-and-llvm-loop-interleave>`__.


`factor(n)` clause
^^^^^^^^^^^^^^^^^^

Specifies the factor by which to unroll the loop, respectively the
number of instructions to group together. If omitted, the compiler
heuristically chooses are factor, including the choice to not
interleave at all.


For example, the code

.. code-block:: c++

  #pragma clang interleave factor(2)
  for (int i = 0; i < N; i+=1) {
    InstructionA(i);
    InstructionB(i);
    InstructionC(i);
  }


is approximately equivalent to

.. code-block:: c++


  for (int i = 0; i < N; i+=2) { // interleaved
    InstructionA(i);
    InstructionA(i+1);
    InstructionB(i);
    InstructionB(i+1);
    InstructionC(i);
    InstructionC(i+1);
  }
  if (N % 2) {
    InstructionA(N-1);
    InstructionB(N-1);
    InstructionC(N-1);
  }
