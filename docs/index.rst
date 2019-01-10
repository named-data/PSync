PSYNC - Partial/Full Sync Library based on BF and IBF
=====================================================

PSync is an ndn-cxx based C++ library for name synchronization that uses

PSync library implements the `PSync protocol <https://named-data.net/wp-content/uploads/2017/05/scalable_name-based_data_synchronization.pdf>`_. It uses Invertible
Bloom Lookup Table (IBLT), also known as Invertible Bloom Filter (IBF), to represent the state
of a producer in partial sync mode and the state of a node in full sync mode. An IBF is a compact data
structure where difference of two IBFs can be computed efficiently.
In partial sync, PSync uses a Bloom Filter to represent the subscription of list of the consumer.
PSync uses `ndn-cxx <https://github.com/named-data/ndn-cxx>`_ library as NDN development
library.

PSync is an open source project licensed under LGPL 3.0 (see ``COPYING.md`` for more
detail). We highly welcome all contributions to the PSync code base, provided that
they can be licensed under LGPL 3.0+ or other compatible license.

Please submit any bugs or issues to the `PSync issue tracker
<https://redmine.named-data.net/projects/PSync/issues>`__.

PSync Documentation
-------------------

.. toctree::
   :hidden:
   :maxdepth: 3

   install
   examples
   RELEASE-NOTES
   releases

-  :doc:`install`
-  :doc:`examples`
-  :doc:`RELEASE-NOTES`
-  :doc:`releases`