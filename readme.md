# Key-Value Store System
![flow diagram](./KV-flowchart.drawio.svg)


## Internal Structure:
- Skiplist Nodes:
```
Key (string)
Value (byte array)
Tombstone (boolean)
```
- Tombstone is used to mark if key is deleted or active without deleting it physically.

- WAL structure:
Write Log Ahead(WAL) is a persistant memory, which used to restore memtable(memory) if server crash or restart.
```
....[total_length][command(0/1/2)][key_len][key][value_len][value]
```

- SSTable structure:
```
[Data block - 1]
[Data block - 2]
..
..

[Bloom Filter]
[Index Block]
[Footer]
```


SST tables stores data in multiple levels[L0-Ln], where L0 stores the Latest data and Ln stores Oldest data. each level contains multiple SST Tables. The SST tables of L0 are overalpping but SST tables of level > L0 are sorted internally as well as globally, which helps to optimize the read operation by looking inside only single SST Table insteaad of all SST tables in level > L0.

