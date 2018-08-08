PSync examples
==============

By default, examples in `examples/` folder are not built.  To enable them, use
`--with-examples` configure option. For example:

    ./waf configure --with-examples
    ./waf

Example binary can be found under `build/examples`:

- Full sync : `psync-full-sync`
- Partial sync : `psync-producer` and `psync-consumer`

If the library is installed to the system using `./waf install` then the examples
are also installed and can be executed directly.

## Partial Sync Example

Partial sync example of PSync has two parts: producer and consumer.
These can be run on a machine after starting NFD:

### Producer

- Enable the logs for producer:

```bash
export NDN_LOG=examples.PartialSyncProducerApp=INFO
```

- Start the producer that will listen on /sync and publish 1 update for /a-0 ... /a-9 each.

```bash
psync-producer /sync /a 10 1
```

- Sample output:

```
1546280442.096296 INFO: [examples.PartialSyncProducerApp] Publish: /a-1/1
1546280456.053138 INFO: [examples.PartialSyncProducerApp] Publish: /a-6/1
1546280458.210415 INFO: [examples.PartialSyncProducerApp] Publish: /a-8/1
1546280469.954134 INFO: [examples.PartialSyncProducerApp] Publish: /a-5/1
1546280472.487425 INFO: [examples.PartialSyncProducerApp] Publish: /a-2/1
1546280473.466515 INFO: [examples.PartialSyncProducerApp] Publish: /a-7/1
1546280481.882258 INFO: [examples.PartialSyncProducerApp] Publish: /a-0/1
1546280484.201229 INFO: [examples.PartialSyncProducerApp] Publish: /a-4/1
1546280489.348968 INFO: [examples.PartialSyncProducerApp] Publish: /a-9/1
1546280491.420391 INFO: [examples.PartialSyncProducerApp] Publish: /a-3/1
```

### Consumer

- In a new terminal, enable the logs for consumer:

```bash
export NDN_LOG=examples.PartialSyncConsumerApp=INFO
```

- Run the consumer to subscribe to 5 random prefixes from the publisher on /sync

```bash
psync-consumer /sync 5
```

- Sample output from the consumer shows that it received updates only
for the subscribed prefixes:

```
1546280436.502769 INFO: [examples.PartialSyncConsumerApp] Subscribing to: /a-7
1546280436.502888 INFO: [examples.PartialSyncConsumerApp] Subscribing to: /a-9
1546280436.502911 INFO: [examples.PartialSyncConsumerApp] Subscribing to: /a-8
1546280436.502934 INFO: [examples.PartialSyncConsumerApp] Subscribing to: /a-4
1546280436.502956 INFO: [examples.PartialSyncConsumerApp] Subscribing to: /a-5
1546280458.211188 INFO: [examples.PartialSyncConsumerApp] Update: /a-8/1
1546280469.954886 INFO: [examples.PartialSyncConsumerApp] Update: /a-5/1
1546280473.467116 INFO: [examples.PartialSyncConsumerApp] Update: /a-7/1
1546280484.256181 INFO: [examples.PartialSyncConsumerApp] Update: /a-4/1
1546280489.349793 INFO: [examples.PartialSyncConsumerApp] Update: /a-9/1
```

## Full Sync Example

To demonstrate full sync mode of PSync, ``psync-full-sync``
can be run on a machine after starting NFD:

- Enable the logs for full sync:

```bash
export NDN_LOG=examples.FullSyncApp=INFO
```

- Run the full sync example with sync prefix /sync, user prefix /a,
and publish three updates for each user prefix: /a-0 and /a-1. This will simulate node a.

```bash
psync-full-sync /sync /a 2 3
```

- Repeat for another user prefix, to simulate node b:

```bash
psync-full-sync /sync /b 2 3
```

We should see that node a and node b have received each other's updates.

- Sample output from node a shows that it received all updates
from node b successfully:

```
1546282730.759387 INFO: [examples.FullSyncApp] Update /b-1/1
1546282741.143225 INFO: [examples.FullSyncApp] Publish: /a-1/1
1546282749.375854 INFO: [examples.FullSyncApp] Publish: /a-0/1
1546282750.263246 INFO: [examples.FullSyncApp] Update /b-0/1
1546282765.875118 INFO: [examples.FullSyncApp] Update /b-1/2
1546282783.777807 INFO: [examples.FullSyncApp] Publish: /a-0/2
1546282794.565507 INFO: [examples.FullSyncApp] Publish: /a-0/3
1546282794.896895 INFO: [examples.FullSyncApp] Publish: /a-1/2
1546282803.839416 INFO: [examples.FullSyncApp] Update /b-0/2
1546282804.785867 INFO: [examples.FullSyncApp] Update /b-1/3
1546282845.273772 INFO: [examples.FullSyncApp] Publish: /a-1/3
1546282855.102790 INFO: [examples.FullSyncApp] Update /b-0/3
```

- Sample output from node b:

```
1546282730.758296 INFO: [examples.FullSyncApp] Publish: /b-1/1
1546282741.144027 INFO: [examples.FullSyncApp] Update /a-1/1
1546282749.376543 INFO: [examples.FullSyncApp] Update /a-0/1
1546282750.262244 INFO: [examples.FullSyncApp] Publish: /b-0/1
1546282765.296005 INFO: [examples.FullSyncApp] Publish: /b-1/2
1546282783.778769 INFO: [examples.FullSyncApp] Update /a-0/2
1546282794.566485 INFO: [examples.FullSyncApp] Update /a-0/3
1546282795.374339 INFO: [examples.FullSyncApp] Update /a-1/2
1546282803.838394 INFO: [examples.FullSyncApp] Publish: /b-0/2
1546282804.033214 INFO: [examples.FullSyncApp] Publish: /b-1/3
1546282845.274680 INFO: [examples.FullSyncApp] Update /a-1/3
1546282855.101780 INFO: [examples.FullSyncApp] Publish: /b-0/3
```