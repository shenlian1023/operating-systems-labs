# Implementation notes

[Project overview](../README.md)

Code excerpts omit some inline comments. The linked source files contain the implementation and its follow-up fixes.

## Giving a shared mailbox an ownership protocol

Source: [sender initialization and transfer loop](../lab1-ipc/sender.c), [receiver loop](../lab1-ipc/receiver.c).

The shared-memory path stores a length followed by message bytes in a mapped region. That region alone cannot tell the sender when the receiver has finished reading. Two named semaphores provide the handoff:

```c
g_sem_sender   = sem_open(SEM_SENDER_NAME,   O_CREAT, 0600, 1);
if (g_sem_sender == SEM_FAILED) die("sem_open sender");
g_sem_receiver = sem_open(SEM_RECEIVER_NAME, O_CREAT, 0600, 0);
if (g_sem_receiver == SEM_FAILED) die("sem_open receiver");
```

The sender waits on `g_sem_sender`, writes through `send`, then posts `g_sem_receiver`. The receiver waits on `g_sem_receiver`, reads through `receive`, and posts `g_sem_sender` after an ordinary message. The terminal `EOF` or `__EXIT__` message exits the receiver instead of starting another transfer.

Mode 1 uses `mq_send`/`mq_receive`; mode 2 uses shared-memory copies. Keeping the same handoff protocol makes both paths easy to trace, though it also serializes transfers. This implementation is not a pipelined queue or a multiple-producer mailbox. Existing objects retain their semaphore values when reopened with `O_CREAT`; the initial values above assume a clean start. Fixed object names also require one sender/receiver pair at a time.

The timer starts after `sem_wait`. It measures the selected send/receive call, not the time spent waiting for the peer or the complete transfer. A comparison of complete IPC latency would need a different measurement boundary and repeated runtime checks. Follow-up sender/receiver runs are recorded in the [validation notes](validation-2026-09-29.md); an unresolved 1,024-byte payload boundary issue remains.

## Protecting a counter with an exchange loop

Source: [`spin_lock`, `spin_unlock`, and `thread`](../lab3-threads/1_2/1_2.c).

The custom lock uses 1 for unlocked and 0 for locked. Its acquisition loop exchanges 0 with the shared lock and inspects the previous value in `eax`:

```c
"loop:\n\t"
"mov $0, %%eax\n\t"
"xchg %%eax, %[lock]\n\t"
"cmp $0, %%eax\n\t"
"je loop\n\t"
```

An old value of 0 means the lock was already held, so the loop retries. An old value of 1 allows the thread to increment the counter; unlock exchanges 1 back into the lock. Two threads each attempt 10,000 increments, making 20,000 the intended final value.

The exercise connects an instruction-level exchange to a critical section. Busy waiting spends CPU time under contention, and the assembly is x86-specific. `volatile` alone is not a portable synchronization primitive. Both counter variants passed their 100-run course checks in the recorded environment. That result does not establish fairness, portability, or correctness under every compiler optimization; the inline-assembly constraints still need review before general reuse.

## Translating a file position into an extent and offset

Source: [`osfs_read` and `osfs_write`](../lab4-osfs/file.c), [storage constants and inode layout](../lab4-osfs/osfs.h).

The filesystem connects VFS file operations to memory-backed extents. Each configured extent contains two 4,096-byte blocks. Division selects the extent; the remainder selects the byte position inside it:

```c
loff_t index = *ppos / (MAX_CONTINUE_BLOCKS * OSFS_BLOCK_SIZE), offset = *ppos % (MAX_CONTINUE_BLOCKS * OSFS_BLOCK_SIZE);
```

If a request crosses that extent boundary, the code places the first segment length and the remaining segment lengths in `lens`. Reads use `copy_to_user`; writes use `copy_from_user`. Each successful segment advances the file position and user-buffer pointer. The write path also updates the VFS inode and its private size/timestamp fields.

The follow-up test exposed a naming collision: the Linux headers defined `BLOCK_SIZE` before the project's guarded definition. Preprocessed code used 1,024-byte blocks, so the two extents could hold only 4 KiB. A 10 KiB copy produced a 4 KiB file even though the old write path returned the requested length. Naming the constant `OSFS_BLOCK_SIZE` restores the intended 4 KiB blocks without redefining a kernel macro.

The write path now checks the position before indexing an extent and clips requests to the remaining capacity:

```c
if (*ppos < 0)
    return -EINVAL;
if (!len)
    return 0;
if (*ppos >= capacity)
    return -EFBIG;
len = min_t(size_t, len, capacity - *ppos);
```

A request that starts inside the file's 16 KiB capacity can return a short write; a subsequent write at the limit returns `EFBIG`. Allocation errors are checked before an extent is marked allocated. If a later extent cannot be allocated, the code updates metadata for the bytes already copied and returns that partial count; retrying at the unallocated extent returns `ENOSPC` rather than using an invalid block address. Reads also stop at the configured capacity.

The [runtime regression test](../lab4-osfs/test_readback.py) verifies these behaviors against a mounted module using real read/write syscalls and nonzero data. All five test groups passed on Linux `6.14.0-37-generic`. This is still a small memory-backed course filesystem: durable storage, concurrent access, sparse files, truncation and arbitrary user-copy faults are outside this verification.

## Technical skills practiced

The labs connect POSIX IPC and semaphore handoff, thread critical sections and machine instructions, and VFS callbacks and storage addressing. The Lab 4 follow-up also links a user-visible truncation failure to kernel macro expansion, then checks the repair through byte-for-byte readback and capacity/error cases. The [validation record](validation-2026-09-29.md) separates the observed results from remaining source issues; a successful build or diagram alone is not evidence of correctness.
