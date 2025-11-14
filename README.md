# qthreads-stdexec

This project provides a prototype senders-based API for thre qthreads user-level threading runtime.
All interfaces should currently be considered experimental and subject to change.

Working installations of both [qthreads](https://github.com/sandialabs/qthreads) and [stdexec](https://github.com/NVIDIA/stdexec) are required.
The code here requires several C++26 features so only compilers with preliminary C++26 support are supported.
Gcc 15 and Clang 20 and 21 should all work.

See also: [the C++ standard proposal introducing senders and receivers](https://wg21.link/P2300).

See the COPYRIGHT.md and LICENSE.md files for information about copyright and licensing.
