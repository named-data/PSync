# PSync: Partial and Full Synchronization Library for NDN

![Language](https://img.shields.io/badge/C%2B%2B-14-blue.svg)
[![Build Status](https://travis-ci.org/named-data/PSync.svg?branch=master)](https://travis-ci.org/named-data/PSync)
![Latest Version](https://img.shields.io/github/tag/named-data/PSync.svg?color=darkkhaki&label=latest%20version)

The PSync library implements the
[PSync protocol](https://named-data.net/wp-content/uploads/2017/05/scalable_name-based_data_synchronization.pdf).
It uses Invertible Bloom Lookup Table (IBLT), also known as Invertible Bloom Filter (IBF),
to represent the state of a producer in partial sync mode and the state of a node in full
sync mode. An IBF is a compact data structure where difference of two IBFs can be computed
efficiently. In partial sync, PSync uses a Bloom Filter to represent the subscription list
of the consumer.

PSync uses the [ndn-cxx](https://github.com/named-data/ndn-cxx) library.

## Installation

### Prerequisites

* [ndn-cxx and its dependencies](https://named-data.net/doc/ndn-cxx/current/INSTALL.html)

### Build

To build PSync from source:

    ./waf configure
    ./waf
    sudo ./waf install

To build on memory constrained platform, please use `./waf -j1` instead of `./waf`. The
command will disable parallel compilation.

If configured with tests (`./waf configure --with-tests`), the above commands will also
generate unit tests that can be run with `./build/unit-tests`.

## Reporting bugs

Please submit any bug reports or feature requests to the
[PSync issue tracker](https://redmine.named-data.net/projects/psync/issues).

## Contributing

We greatly appreciate contributions to the PSync code base, provided that they are
licensed under the LGPL 3.0+ or a compatible license (see below).
If you are new to the NDN software community, please read the
[Contributor's Guide](https://github.com/named-data/.github/blob/master/CONTRIBUTING.md)
to get started.

## License

PSync is an open source project licensed under the LGPL version 3.
See [`COPYING.md`](COPYING.md) for more information.
