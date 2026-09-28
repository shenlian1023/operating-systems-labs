# Implementation notes

[Project overview](../README.md)

The source excerpts omit original comments; executable text is unchanged.

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

The timer starts after `sem_wait`. It measures the selected send/receive call, not the time spent waiting for the peer or the complete transfer. A comparison of complete IPC latency would need a different measurement boundary and repeated runtime checks. The recorded local verification compiled the two programs; it did not run the IPC pair concurrently.

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

The exercise connects an instruction-level exchange to a critical section. Busy waiting spends CPU time under contention, and the assembly is x86-specific. `volatile` alone is not a portable synchronization primitive. The inline assembly's memory operand constraints and interaction with compiler optimization need review before treating this as a reusable lock. No fairness, race-free portability, or runtime test result is claimed.

## Translating a file position into an extent and offset

Source: [`osfs_read` and `osfs_write`](../lab4-osfs/file.c), [storage constants and inode layout](../lab4-osfs/osfs.h).

The filesystem connects VFS file operations to memory-backed extents. Each configured extent contains two 4,096-byte blocks. Division selects the extent; the remainder selects the byte position inside it:

```c
loff_t index = *ppos / (MAX_CONTINUE_BLOCKS * BLOCK_SIZE), offset = *ppos % (MAX_CONTINUE_BLOCKS * BLOCK_SIZE);
```

If a request crosses that extent boundary, the code places the first segment length and the remaining segment lengths in `lens`. Reads use `copy_to_user`; writes use `copy_from_user`. Each successful segment advances the file position and user-buffer pointer. The write path also updates the VFS inode and its private size/timestamp fields.

This implements address translation across extents and the user/kernel copy boundary. It remains a small course filesystem with no durable disk storage. Source inspection found unfinished error handling: writes use allocation results without checking `ret`, and extent indices are not checked before every access. A request beyond the configured capacity can also retain the original requested byte count even when fewer bytes are copied. These paths need fixes and boundary tests in a disposable VM before relying on the module. No kernel module was loaded during this documentation review.

## Technical skills practiced

The labs connect POSIX IPC and semaphore handoff, thread critical sections and machine instructions, and VFS callbacks and storage addressing. Their source and course templates support those learning topics. Runtime correctness, numerical correctness of the matrix variants, and kernel stability require separate testing; they should not be inferred from a successful build or a diagram.
