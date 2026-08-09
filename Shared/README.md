# SentinelShared

`SentinelShared` contains infrastructure used by Sentinel, SentinelTasks and SentinelStats.

## Locked atomic JSON datastore

`JsonDataStore` is the single persistence boundary for shared Sentinel JSON files.

For reads it:

1. opens `<data-file>.lock`,
2. acquires a shared `flock()` lock,
3. reads and parses the JSON document,
4. releases the lock.

For updates it performs one exclusive transaction:

```text
open persistent sidecar lock
-> LOCK_EX
-> read latest JSON root
-> mutate only the caller's section
-> write same-directory temporary file
-> fsync temporary file
-> atomic rename over the JSON file
-> fsync parent directory
-> unlock
```

The lock is deliberately stored in a persistent sidecar file such as:

```text
current_data.json.lock
```

rather than on `current_data.json` itself. The JSON file is replaced with `rename()` during an atomic commit, so locking the replaceable JSON inode would not provide a stable synchronization object.

Sentinel writes its `sentinel`/legacy task section through this datastore. SentinelTasks writes only `sentinelTasks`. SentinelStats uses the shared locked read path.

This prevents Sentinel and SentinelTasks from overwriting each other's unrelated sections when they autosave concurrently. Two independent processes editing the same logical section still use last-writer-wins application semantics; conflict merging within one section is outside the file-locking layer.

## Regression test

With `BUILD_TESTING` enabled (the CMake default), build and run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

`SentinelShared.ConcurrentJsonTransactions` forks two writers that repeatedly update different keys in the same JSON file and verifies that all updates survive.
