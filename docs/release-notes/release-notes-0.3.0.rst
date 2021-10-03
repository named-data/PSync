PSync version 0.3.0
-------------------

*Release date: January 3, 2021*

The build requirements have been increased to require Clang >= 4.0, Xcode >= 9.0, and Python >= 3.6.
Meanwhile, it is *recommended* to use GCC >= 7.4.0 and Boost >= 1.65.1.
This effectively drops official support for Ubuntu 16.04 when using distribution-provided Boost
packages -- PSync may still work on this platform, but we provide no official support for it.

We have taken some steps to be endian safe but PSync is not completely endian safe yet (:issue:`4818`)

New features
^^^^^^^^^^^^

- **breaking** Consumer: change hello data callback to include sequence number (:issue:`5122`)

Improvements and bug fixes
^^^^^^^^^^^^^^^^^^^^^^^^^^

- **breaking** IBLT: make encoding endian safe (:issue:`5076`)
- Reset cached wire encoding after adding names (:issue:`5083`)
- Consumer reacts faster on sync Interest timeout (:issue:`5124`)
- Move private classes and functions to ``psync::detail`` namespace
- Improved unit tests
