# Serialized I/O Feature for HDD Performance

## Problem

When hashing multiple files in parallel using multiple threads on a traditional hard disk drive (HDD), performance can be significantly degraded due to **disk head thrashing**. While multiple threads can improve CPU utilization on SSDs, HDDs experience severe performance penalties when the disk head must constantly seek between different files.

### Symptoms
- Low CPU usage (~30%) despite using multiple threads
- Slower overall hashing speed than expected
- Disk activity is the bottleneck, not CPU

### Root Cause
- **HDDs**: Sequential read speed ~100-150 MB/s, Random read ~1-5 MB/s (20-100x slower)
- **Multiple threads** reading different files = constant disk seeking
- Each 100KB chunk read requires repositioning the disk head
- Result: Parallel I/O on HDD serializes anyway due to physical limitations

## Solution

The **Serialized I/O** feature adds a global mutex that ensures only one thread reads from disk at a time, while still allowing parallel CPU hashing computation.

### Architecture

```
Without Serialized I/O (Good for SSD):
Thread 1: [Read File A        ]→[Hash A]
Thread 2:    [Read File B      ]→[Hash B]
Thread 3:       [Read File C   ]→[Hash C]
All parallel, disk thrashing on HDD ❌

With Serialized I/O (Good for HDD):
Thread 1: [Read File A]→[Hash A]
Thread 2:              [Read File B]→[Hash B]
Thread 3:                           [Read File C]→[Hash C]
Linear disk reads, parallel CPU hashing ✅
```

### Implementation Details

1. **Global Mutex**: `ed2k::fileIOMutex` - Static mutex shared by all threads
2. **Global Flag**: `ed2k::useSerializedIO` - Enable/disable feature
3. **Locking Strategy**: 
   - Lock acquired at start of file I/O
   - Lock held during all `file.read()` operations
   - Lock released immediately after file is closed
   - MD4 finalization (CPU work) remains parallel

## Usage

### UI Control

In the Hasher tab, enable the checkbox:
```
☑ Serialize I/O (for HDD)
```

**Tooltip**: "Enable for traditional hard drives to prevent disk head thrashing. Disable for SSDs to allow parallel I/O."

### Programmatic Control

```cpp
// Enable serialized I/O (before starting hasher)
ed2k::setSerializedIO(true);

// Check current state
bool isEnabled = ed2k::getSerializedIO();

// Disable serialized I/O
ed2k::setSerializedIO(false);
```

## When to Use

### Enable Serialized I/O (✅ Recommended)
- **Traditional HDDs**: Spinning disk drives with mechanical read heads
- **Files on same physical disk**: Multiple files on same HDD
- **Small to medium files**: Better linear read patterns
- **Low CPU usage observed**: Indicates I/O bottleneck

### Disable Serialized I/O (❌ Not Recommended)
- **SSDs**: Solid state drives handle parallel I/O efficiently
- **Files on different disks**: Each thread reads from different physical disk
- **Network storage**: NAS/SAN with good parallel I/O support
- **High CPU usage observed**: CPU is bottleneck, not I/O

## Performance Characteristics

### Expected Results on HDD

**Before (Parallel I/O):**
- Multiple threads fighting for disk access
- Constant seeking between files
- Throughput: ~1-5 MB/s per thread (random reads)
- CPU usage: ~30% (threads waiting for I/O)

**After (Serialized I/O):**
- One thread reads sequentially
- No seeking during file read
- Throughput: ~100-150 MB/s sequential read
- CPU usage: Should increase (less I/O wait)

### Expected Results on SSD

**Before (Parallel I/O):**
- Multiple threads reading simultaneously
- SSD handles parallel access efficiently
- Full CPU utilization

**After (Serialized I/O):**
- May reduce performance slightly
- SSDs benefit from parallel access
- Keep disabled for SSDs

## Technical Details

### Code Changes

#### ed2k.h
```cpp
class ed2k : public QObject, private MD4
{
    // ...
private:
    static QMutex fileIOMutex;  // Global mutex
    static bool useSerializedIO; // Enable flag
public:
    static void setSerializedIO(bool enabled);
    static bool getSerializedIO();
    // ...
};
```

#### ed2k.cpp
```cpp
int ed2k::ed2khash(QString filepath)
{
    // ...
    QMutex *ioMutex = useSerializedIO ? &fileIOMutex : nullptr;
    
    if (ioMutex)
        ioMutex->lock();
    
    file.open(QIODevice::ReadOnly);
    // ... read file in chunks ...
    file.close();
    
    if (ioMutex)
        ioMutex->unlock();
    
    // MD4 finalization (parallel, not locked)
    Final();
    // ...
}
```

### Thread Safety

- **Static mutex**: Shared across all hasher threads
- **Unlocking on all paths**: Error, stop, success all unlock properly
- **No deadlocks**: Mutex acquired and released in same function
- **No race conditions**: Flag is set before threads start

## Testing

### Test Suite

The feature includes comprehensive tests in `tests/test_serialized_io.cpp`:

1. **testSerializedIOFlag**: Verify get/set functionality
2. **testSerializedIODisabled**: Hash with feature disabled
3. **testSerializedIOEnabled**: Hash with feature enabled
4. **testMultipleFilesSerializedIO**: Verify identical hashes regardless of setting

### Running Tests

```bash
cd build
cmake ..
make test_serialized_io
./tests/test_serialized_io
```

## Limitations

1. **Not automatic**: User must manually enable via checkbox
2. **Global setting**: Applies to all files in current batch
3. **No dynamic switching**: Can't change mid-hashing session
4. **Memory overhead**: None (just a boolean flag and mutex)

## Future Enhancements

Potential improvements:
1. **Auto-detection**: Detect HDD vs SSD and enable automatically
2. **Per-disk control**: Different settings for different physical disks
3. **Dynamic adjustment**: Monitor I/O wait and adjust on the fly
4. **Statistics**: Show I/O throughput to help users decide

## References

- **Issue**: User reported 30% CPU usage with 6 threads on HDD
- **Implementation**: Global mutex for file I/O serialization
- **Default**: Disabled (backward compatible)
- **Changelog**: Added in 2025-11-13

## Related Documentation

- [MULTITHREADED_HASHER.md](MULTITHREADED_HASHER.md) - Multi-threaded hashing architecture
- [HASHER_WORKFLOW_OPTIMIZATION.md](HASHER_WORKFLOW_OPTIMIZATION.md) - Batch processing optimizations
