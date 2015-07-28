Anteater is a tool that detects networking bugs through static analysis of the data plane state of the networking devices. Anteater translates high-level network invariants into boolean satisfiability problems (SAT), checks them against network state using a SAT solver, and reports counterexamples if violations have been found.

For more information please read the paper published in [SIGCOMM'2011](http://conferences.sigcomm.org/sigcomm/2011):

Haohui Mai et al. [Debugging the data plane with Anteater](http://conferences.sigcomm.org/sigcomm/2011/papers/sigcomm/p290.pdf). In the Proceedings of the ACM SIGCOMM 2011 conference.

Building
========

Anteater requires the following dependency to build and run:

* CMake 2.6 or higher
* LLVM 2.9
* Boost 1.42
* Coreutils
* A SAT solver (currently Anteater supports Boolector, Yices and Z3)

To build Anteater:

```
$ cmake
$ make
```

You can find the invariant checkers described in the paper under the `tools/scripts` directory and run them to detect networking issues.

Contribution
============

Your pull requests are appreciated!
