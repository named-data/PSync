PSync version 0.1.0
-------------------

*Release date: January 25, 2019*

Version 0.1.0 is the initial release of PSync. The PSync library implements the `PSync protocol
<https://named-data.net/wp-content/uploads/2017/05/scalable_name-based_data_synchronization.pdf>`__.
It uses Invertible Bloom Lookup Table (IBLT), also known as Invertible Bloom Filter (IBF), to represent
the state of a producer in partial sync mode and the state of a node in full sync mode. An IBF is a
compact data structure where difference of two IBFs can be computed efficiently.
In partial sync, PSync uses a Bloom Filter to represent the subscription of list of the consumer.
PSync uses the `ndn-cxx <https://github.com/named-data/ndn-cxx>`__ library as NDN development
library.

PSync is an open source project licensed under LGPL 3.0. We highly welcome all contributions to the PSync code base, provided that they can be licensed under LGPL 3.0+ or other compatible license.

PSync supports the following features:

- **Partial Synchronization**

  + Allows consumers to subscribe to a subset of a producer's offered prefixes.
  + Consumer can sync with any replicated producer in the same sync group.
  + A PartialProducer class to publish prefixes and updates.
  + A Consumer class to subscribe to the producer (hello) and listen for updates (sync).

- **Full Synchronization**

  + Similar to ChronoSync, allows full synchronization of names amongst all the nodes in the sync group.
  + FullProducer class can be used start syncing with a sync group.

- **Miscellaneous**

  + Uses ndn-cxx ``SegmentFetcher`` to segment hello and sync data.
  + Provide a SegmentPublisher that can be used by other NDN applications.
  + Examples are provided in ``examples`` folder to show how to use the library.
