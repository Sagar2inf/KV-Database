<<<<<<< HEAD
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
=======
# LSM-based Key-Value Store

A persistent key-value database built from scratch in C++. Uses a Log-Structured Merge Tree (LSM) internally — same general idea as what RocksDB and LevelDB do, just written by hand to actually understand how the thing works.

![Architecture](./KV-flowchart.drawio.svg)

---

## Why I built this

I wanted to understand what happens between `PUT key value` and the bytes hitting disk. Most databases are black boxes. I started with a skiplist and a WAL, got addicted, and kept going. It now has SSTables, bloom filters, a sparse index, compaction, WAL recovery, and a working TCP server.

Is it RocksDB? No. Does it work? Yes.

---

## How it works internally

### Write path

When you `SET name=sagar`, this is what happens:

1. The server parses and validates the command
2. The entry is serialized and appended to the **WAL** (Write-Ahead Log) — fsynced to disk immediately so a crash doesn't lose the write
3. The entry is inserted into the **MemTable** (a skiplist in memory, kept sorted by key)
4. Once the MemTable hits ~4MB, it gets flushed to a new **SSTable** file on disk, and the WAL is truncated

### Read path

1. Check the MemTable first (O(log n) skiplist lookup)
2. If not found, scan SSTables from newest to oldest
3. Each SSTable has a **bloom filter** — if the filter says the key isn't there, skip the file entirely (no disk read)
4. If the bloom filter passes, use the **sparse index** to jump to roughly the right position in the file, then do a short sequential scan

Deletes write a **tombstone** — a marker that says "this key is gone." That tombstone shadows any older value in any SSTable until compaction cleans it up.

### Compaction

L0 files are raw memtable dumps and can have overlapping key ranges. When you accumulate 4 of them, the compaction kicks in: all L0 files plus any existing L1 files get merge-sorted into a single new L1 file. Duplicate keys are resolved by keeping the newest version. This is what keeps read performance from degrading over time.

### WAL recovery

On startup, the server loads all existing SSTable files first, then replays the WAL on top. That gives you back exactly the state at the time of the crash or shutdown.

---

## Data format

**WAL entry** (all integers big-endian):
```
[total_len : 4 bytes]
[command   : 1 byte]   1=SET, 3=DELETE
[key_len   : 4 bytes]
[key       : variable]
[value_len : 4 bytes]  only for SET
[value     : variable] only for SET
```

**SSTable file layout**:
```
[magic header : 8 bytes]  "SSTR1.0\0"
[entry count  : 4 bytes]
[data section]            sorted key-value entries
[bloom filter]            m bits, k hash functions (MurmurHash64 double-hashing)
[sparse index]            one pointer per 16 keys → jump to approximate position
[footer : 40 bytes]       offsets to bloom filter, index, entry count, level, timestamp
```

---

## Build

You need a C++17 compiler and cmake.

```bash
git clone <repo>
cd Distributed-Key-Value-Database
mkdir build && cd build
cmake ..
make -j$(nproc)
```

This produces two binaries: `kv-server` and `kv-client`.

On Windows with MinGW (MSYS2):

```bash
g++ -std=c++17 -O2 disk/wal.cpp disk/sstable.cpp disk/sstable_manager.cpp \
    memory/skiplist.cpp memtable/memtable.cpp services/service.cpp \
    transport/transport.cpp routers/router.cpp server.cpp \
    -o kv-server.exe -lws2_32

g++ -std=c++17 -O2 client.cpp transport/transport.cpp -o kv-client.exe -lws2_32
```

---

## Running

Start the server from the `build/` directory. It creates a `data/` folder there for the WAL and SSTables.

```bash
./kv-server
```

```
Recovering WAL...
Recovery done. 0 live keys in memtable.
Server listening on port 9000
```

Connect with the client in another terminal:

```bash
./kv-client
```

To connect to a remote server:

```bash
./kv-client 192.168.1.10 9000
```

---

## Commands

```
SET key=value     store a value
GET key           retrieve a value
DELETE key        delete a key
exit              close the client
```

Keys and values are printable ASCII. No colons in keys.

```
SET username=john
OK

SET score=42
OK

GET username
john

DELETE score
OK

GET score
NOT_FOUND
```

You can also pipe a script:

```bash
./kv-client < commands.txt
```

---

## Data directory

```
build/
└── data/
    ├── wal/
    │   └── walfile          # append-only log, truncated after each flush
    └── sstables/
        ├── L0_<ts>.sst      # fresh flushes from memtable
        └── L1_<ts>.sst      # compacted, deduplicated
```

Nothing in `data/` should be hand-edited. If the server crashes mid-write and the WAL looks corrupted, it stops replaying at that point and logs a message — it won't silently serve wrong data.

---

## Configuration

Edit `config.hpp` and rebuild:

```cpp
namespace config {
    constexpr int    PORT                 = 9000;
    constexpr int    MAX_CLIENTS          = 256;
    constexpr size_t MEMTABLE_FLUSH_BYTES = 4 * 1024 * 1024; // 4 MB
    constexpr int    L0_COMPACT_TRIGGER   = 4;
    // ...
}
```

---

## Architecture overview

```
client (TCP)
    │
    ▼
Transport          length-framed binary protocol
    │
    ▼
Service            parse and validate the command string
    │
    ▼
Router             route to memtable, build WAL entry
    │
    ├──► WAL        append + fdatasync before touching memory
    │
    └──► Memtable
              │
              ├──► Skiplist       in-memory sorted index (shared_mutex for thread safety)
              │
              └──► SSTableManager
                        │
                        ├── L0 SSTables  (bloom filter + sparse index each)
                        └── L1 SSTables  (compacted)
```

---

## What's not here yet

- Distributed replication — the plan is to layer Raft on top once the single-node store is solid
- Range queries / SCAN — the skiplist and SSTables are both sorted so this is doable, just not wired up yet
- Compression — values are stored raw, no snappy/lz4
- Authentication — it's plaintext TCP, don't expose this to the internet as-is

---

## Tech

- C++17, no external dependencies
- POSIX file I/O (`open`, `pread`, `fdatasync`) on Linux; Windows-compatible via a thin shim in `platform.hpp`
- MurmurHash64 for bloom filters
- `poll()`/`WSAPoll` for the event loop (no `select()`)
>>>>>>> 2549828 (required changes)
