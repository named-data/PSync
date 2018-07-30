PSYNC - Partial and Full Synchronization Library for NDN
========================================================

If you are new to the NDN community of software generally, read the
[Contributor's Guide](https://github.com/named-data/NFD/blob/master/CONTRIBUTING.md).

PSync library implements the [PSync protocol](https://named-data.net/wp-content/uploads/2017/05/scalable_name-based_data_synchronization.pdf). It uses Invertible
Bloom Lookup Table (IBLT), also known as Invertible Bloom Filter (IBF), to represent the state
of a producer in partial sync mode and the state of a node in full sync mode. An IBF is a compact data
structure where difference of two IBFs can be computed efficiently.
In partial sync, PSync uses a Bloom Filter to represent the subscription of list of the consumer.
PSync uses [ndn-cxx](https://github.com/named-data/ndn-cxx) library as NDN development
library.

PSync is an open source project licensed under LGPL 3.0 (see `COPYING.md` for more
detail).  We highly welcome all contributions to the PSync code base, provided that
they can be licensed under LGPL 3.0+ or other compatible license.

Feedback
--------

Please submit any bugs or issues to the **PSync** issue tracker:

* https://redmine.named-data.net/projects/psync

Installation instructions
-------------------------

### Prerequisites

Required:

* [ndn-cxx and its dependencies](https://named-data.net/doc/ndn-cxx/)

### Build

To build PSync from the source:

    ./waf configure
    ./waf
    sudo ./waf install

To build on memory constrained platform, please use `./waf -j1` instead of `./waf`. The
command will disable parallel compilation.

If configured with tests: `./waf configure --with-tests`), the above commands will also
generate unit tests in `./build/unit-tests`
