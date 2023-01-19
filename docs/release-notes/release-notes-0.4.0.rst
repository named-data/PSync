PSync version 0.4.0
-------------------

*Release date: January 18, 2023*

Important changes and new features
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- The minimum build requirements have been increased as follows:

  - Either GCC >= 7.4.0 or Clang >= 6.0 is required on Linux
  - On macOS, Xcode 11.3 or later is recommended; older versions may still work but are not
    officially supported
  - Boost >= 1.65.1 and ndn-cxx >= 0.8.1 are required on all platforms
  - Sphinx 4.0 or later is required to build the documentation

- *(breaking change)* Switch to C++17
- *(breaking change)* The Name TLV value is now hashed directly instead of being converted
  to URI format first (:issue:`4838`)
- Add ``incomingFace`` field to missing data notifications (:issue:`3626`)
- *(breaking change)* Add ``ndn::KeyChain`` parameter to the producer API
- Provide API to remove a subscription in partial sync :psync:`Consumer` (:issue:`5242`)

Improvements and bug fixes
^^^^^^^^^^^^^^^^^^^^^^^^^^

- Use ndn-cxx's ``ndn::util::Segmenter`` class in :psync:`SegmentPublisher`
- Fix compilation against the latest version of ndn-cxx
- Stop using the ``gold`` linker on Linux; prefer instead linking with ``lld`` if installed
- Update waf build system to version 2.0.24
- Various documentation improvements

Known issues
^^^^^^^^^^^^

- We have taken some steps to be endian safe but PSync is not completely endian safe yet
  (:issue:`4818`)
