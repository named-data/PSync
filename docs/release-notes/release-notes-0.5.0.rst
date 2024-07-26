PSync version 0.5.0
-------------------

*Release date: July XX, 2024*

Important changes and new features
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- The build dependencies have been increased as follows:
  - GCC >= 9.3 or Clang >= 7.0 are strongly *recommended* on Linux; GCC 8.x is also known
  to work but is not officially supported
  - Xcode 13 or later is *recommended* on macOS; older versions may still work but are not
  officially supported
  - Boost >= 1.71.0 is *required* on all platforms
- We have officially moved to a modified FullSync algorithim originally designed by Ashlesh
  Gawande as part of `his thesis work
  <https://digitalcommons.memphis.edu/cgi/viewcontent.cgi?article=3162&context=etd>`__.
  These changes are intended to lower delay and overhead when using FullSync

Improvements and bug fixes
^^^^^^^^^^^^^^^^^^^^^^^^^^

- Constructor options are now passed in as single ``Options`` object (:issue:`5069`)
- :psync:`FullProducer` no longer appends the hash of the IBF to the data name; this had no functional
  purpose (:issue:`5066`)
- Refactoring of IBLT implementation (:issue:`4825`)
- Various adjustments to match ndn-cxx namespace changes
- Update waf build system to version 2.0.27
- Fix building the documentation with Python 3.12 (:issue:`5298`)
- Miscellanous CI and build improvements