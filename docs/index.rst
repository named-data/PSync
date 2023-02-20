PSync: Partial/Full Sync Library based on BF and IBF
====================================================

.. toctree::
   :hidden:
   :maxdepth: 2

   install
   examples
   release-notes
   releases

PSync is a C++ library for name synchronization that implements the `PSync protocol
<https://named-data.net/wp-content/uploads/2017/05/scalable_name-based_data_synchronization.pdf>`__.
It uses Invertible Bloom Lookup Table (IBLT), also known as Invertible Bloom Filter (IBF),
to represent the state of a producer in partial sync mode and the state of a node in full
sync mode. An IBF is a compact data structure where difference of two IBFs can be computed
efficiently. In partial sync, PSync uses a Bloom Filter to represent the subscription of
list of the consumer.

PSync uses the `ndn-cxx <https://github.com/named-data/ndn-cxx>`__ library.

Documentation
-------------

- :doc:`install`
- :doc:`examples`
- :doc:`release-notes`
- :doc:`releases`

Issues
------

Please submit any bug reports or feature requests to the
`PSync issue tracker <https://redmine.named-data.net/projects/psync/issues>`__.

Contributing
------------

Contributions to PSync are greatly appreciated and can be made through our
`Gerrit code review site <https://gerrit.named-data.net/>`__.
If you are new to the NDN software community, please read our `Contributor's Guide
<https://github.com/named-data/.github/blob/main/CONTRIBUTING.md>`__ to get started.

License
-------

PSync is free software: you can redistribute it and/or modify it under the terms of
the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version. See `COPYING.md
<https://github.com/named-data/PSync/blob/master/COPYING.md>`__ for details.
