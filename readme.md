# Key-Value Store System
![flow diagram](./KV-flowchart.drawio.svg)


## Internal Structure:
1) Skiplist Nodes:
```
Key (string)
Value (byte array)
Tombstone (boolean)
```
- Tombstone is used to mark if key is deleted or active without deleting it physically.

2) WAL structure:
Write Log Ahead(WAL) is a persistant memory, which used to restore memtable(memory) if server crash or restart.
```
....[total_length][command(0/1/2)][key_len][key][value_len][value]
```

3) SSTable structure:
structure of each SST file is as follows
```
[Data block - 1] -> [a - d]
[Data block - 2] -> [e - k]
..               -> [l - ..]
..

[Bloom Filter]
[Index Block]
[Footer]
```


SST tables stores data in multiple levels[L0-Ln], where L0 stores the Latest data and Ln stores Oldest data. each level contains multiple SST Tables. The SST tables of L0 are overalpping but SST tables of level > L0 are sorted internally as well as globally, which helps to optimize the read operation by looking inside only single SST Table insteaad of all SST tables in level > L0.

- Bloom Filter: before start searching in page we check if key was ever written in file or not, and proceed for search if bloom filter allows.
- Index Block: it helps to avoid reading whole SST file in memory, we store ```start key of each block - pointer to starting position of each block``` in Index Block, and it helps to get only useful Data Block in memory
- Footer: it helps to get where the Bloom Filter and Index block are.

## Working Mechanism:
- User can do get/set/delete queries in the form of raw bytes, and API-handler handle the query and send it to server.
- First write this query to WAL
- IF SET x = y:
    add query to the skiplist
- IF GET/DELETE x:
    search in memtable (Time Complexity: O(log(number of nodes in memtable)))
    if not found in memtable then check global bloom filter (fast validator) if key exists according to global bloom filter, then search in each level form 0 to n, and it will again take O(log(n)) time for sarching

- When memtable size reach threshold, then move current memtable and WAL `Immutable data` and background job will flush this data to SSTable, If Immutable Memtable found currept, then it using corresponding WAL and again start writing to disk.
- Data from Immutable Memtable is always written in L0 new SST File so that latest data always remain in Lo.
- If number of SST files in level `i` increases then compact it to `i + 1` level using Merge Sort (to ensure that all level > L0 are globally sorted).


## How to Use it:
- inside linux first clone this repo and make a new directory `build` and run the `CMake` commands to start server
```
g++ -std=c++17 client.cpp -o client
./client

SET name sagar
GET name
SET x 42
GET x
```
- Or write a script.kv file and feed it directly to client using `./client < script.kv`
