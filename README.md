# Linux systems programming

Processes exchange messages, threads share work, and a kernel module exposes files through Linux VFS. These NCKU operating-systems labs implement those mechanisms in C, using the course templates.

`C` · `POSIX IPC` · `pthreads` · `procfs` · `Linux VFS`

[Lab map](#what-the-labs-do) · [IPC implementation](#lab-1-one-transfer-two-ipc-mechanisms) · [Thread implementation](#lab-3-shared-state-and-parallel-work) · [VFS implementation](#lab-4-files-without-disk-persistence)

![Independent Linux labs and one-message semaphore handoff](assets/method-overview.png)

The labs are separate exercises. The lower panel shows mailbox ownership returning to the sender after the receiver consumes an ordinary message. [Figure sources and scope](docs/method-overview-illustrated.md).

## What the labs do

| Lab | Observable behavior | Implementation |
| --- | --- | --- |
| 1: inter-process communication | A sender transfers text lines to a receiver through either a message queue or shared memory | [lab1-ipc](lab1-ipc) |
| 3: threads and synchronization | Threads update a shared counter, partition matrix work, and expose thread information through procfs | [lab3-threads](lab3-threads) |
| 4: in-memory filesystem | Files and directories use VFS operations with memory-backed storage, including reads/writes across block boundaries | [lab4-osfs](lab4-osfs) |

The repository contains **Labs 1, 3, and 4**, not the complete course. The Lab 1 sender and receiver compiled on Ubuntu 22.04 under WSL in the local check recorded on 2026-09-28. Runtime behavior and the kernel modules were not validated in that check; no speedup or official score is claimed.

## Lab 1: one transfer, two IPC mechanisms

The same sender/receiver interface selects either path with a mode flag. Named semaphores coordinate the programs. Timing brackets the send or receive call after the semaphore wait, so the recorded values exclude synchronization waiting and do not measure end-to-end transfer latency.

The [sender](lab1-ipc/sender.c) creates a semaphore for each side of the handoff (original inline comments omitted):

```c
g_sem_sender   = sem_open(SEM_SENDER_NAME,   O_CREAT, 0600, 1);
if (g_sem_sender == SEM_FAILED) die("sem_open sender");
g_sem_receiver = sem_open(SEM_RECEIVER_NAME, O_CREAT, 0600, 0);
if (g_sem_receiver == SEM_FAILED) die("sem_open receiver");
```

The receiver returns permission after consuming an ordinary message. Starting the sender semaphore at 1 and the receiver semaphore at 0 creates a one-message handshake, which prevents a normal single-pair run from overwriting an unread shared-memory message. This is an exercise in process coordination and measurement boundaries. See the [implementation notes](docs/implementation.md) for the handshake, the `xchg` lock, and VFS read/write addressing.

The protocol is `wait(sender) → send → post(receiver)` on one side and `wait(receiver) → receive → post(sender)` on the other. Shared memory stores a length followed by bytes; the message-queue path uses `mq_send` and `mq_receive`. Terminal messages stop the receiver instead of handing back another permit. Existing named objects can retain semaphore state, so the initial values assume a clean start.

Build on Linux:

```bash
cd lab1-ipc
gcc -O3 -Wall sender.c -o sender -pthread -lrt
gcc -O3 -Wall receiver.c -o receiver -pthread -lrt
```

Start the sender, then the receiver in another terminal:

```bash
# Terminal 1
./sender 1 input.txt
# Terminal 2
./receiver 1
```

Use `2` in both commands for shared memory. Run one pair at a time: fixed IPC object names can make concurrent runs interfere.

## Lab 3: shared state and parallel work

| Directory | Mechanism |
| --- | --- |
| `1_1` | Shared counter protected by a pthread spinlock |
| `1_2` | Custom spinlock using x86 `xchg` |
| `2_1_2_2` | Single-thread and two-thread matrix multiplication |
| `3_1`, `3_2` | Row-partitioned matrix work and procfs kernel modules |

These exercises separate synchronization from work partitioning. They do not establish that two threads are always faster or that the implementations are race-free.

The [custom lock](lab3-threads/1_2/1_2.c) uses 1 for unlocked and 0 for locked. Its acquisition loop exchanges 0 with the shared lock, then checks the previous value in `eax` (original comments omitted):

```c
"loop:\n\t"
"mov $0, %%eax\n\t"
"xchg %%eax, %[lock]\n\t"
"cmp $0, %%eax\n\t"
"je loop\n\t"
```

An old value of 0 retries; an old value of 1 permits the counter update. Unlock exchanges 1 back into the lock. This connects an instruction-level exchange to a thread critical section. Busy waiting consumes CPU under contention, and the x86 inline-assembly constraints need review before reusing this as a general lock. `volatile` alone does not establish portable synchronization.

The `3_2` variant references a missing `3_2_Config.h` and local matrix inputs, so it is not self-contained. Some matrix variants accumulate into `malloc`-allocated output without explicitly zeroing it. Check numerical correctness before comparing execution times.

## Lab 4: files without disk persistence

The `osfs` module connects superblock, inode, directory, and file operations to Linux VFS. Memory-backed blocks hold file contents, including data spanning multiple blocks. Contents are not durable disk storage.

In [file.c](lab4-osfs/file.c), division selects an extent and the remainder selects the byte position inside it:

```c
loff_t index = *ppos / (MAX_CONTINUE_BLOCKS * BLOCK_SIZE), offset = *ppos % (MAX_CONTINUE_BLOCKS * BLOCK_SIZE);
```

The [configuration](lab4-osfs/osfs.h) gives each extent two 4,096-byte blocks. A crossing request is split into lengths in `lens`; each copy advances the file position and user-buffer pointer. Reads use `copy_to_user`, writes use `copy_from_user`, and writes synchronize size/timestamp fields with the VFS inode. These operations practice storage addressing and the user/kernel boundary.

Allocation-return checks, extent-index bounds, and byte counts for oversized requests still need attention. The [extended notes](docs/implementation.md) describe these source-level issues; successful compilation does not establish filesystem reliability.

The [original test notes](lab4-osfs/docs/original-test-notes.md) describe mount and file-operation checks. Building needs kernel headers matching the target kernel. Load and test the module only in a disposable Linux VM, not on the host computer.

## Environment and validation limits

Lab 1's build check did not include a concurrent IPC run. Kernel modules were not loaded. The custom assembly is architecture-dependent, and kernel APIs can differ between releases. Source inspection and archived answers alone do not prove runtime correctness, performance improvement, or kernel stability.
