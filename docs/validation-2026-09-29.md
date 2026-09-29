# Validation — 2026-09-29

[Project overview](../README.md) · [Lab 4 test instructions](../lab4-osfs/README.md)

These are follow-up checks, not an official grade or a claim that the original submission already passed them.

## Environment and observations

VirtualBox Ubuntu VM, x86-64, Linux `6.14.0-37-generic`, GCC 13.3.0. Tests used isolated copies of the local coursework; the verified Lab 4 fix was subsequently synchronized into this repository.

| Part | Observed result | Limit |
| --- | --- | --- |
| Lab 1 message queue and shared memory | Sender and receiver both exited normally with the supplied input | Does not cover every payload or reused semaphore state |
| Lab 3 counter variants | Both 100-run course judges reported `Success` | Does not prove lock portability or fairness |
| Lab 3 matrix variants | Single-thread and ten-run two-thread judges reported `Success` | No speedup measured; source initialization concerns remain |
| Lab 3 procfs variants | Both modules built, loaded, returned thread fields, and were unloaded | Returned timing fields are not a validated performance comparison |
| Lab 4 corrected module | All five regression groups passed | Only the documented operations and error cases were tested |

Separate Ubuntu 22.04/WSL checks verified Lab 1's supplied 100-message input, five repeated 100-message/256-byte transfers per mode, and termination without an explicit EOF line. A 1,024-byte payload case failed in both modes; that issue is unresolved and is not part of the Lab 4 repair.

## Lab 4: failure, repair, readback

The initial 10 KiB copy produced a file of only 4,096 bytes. Kernel preprocessing showed that the generic `BLOCK_SIZE` name expanded to `(1<<10)`, not the intended 4,096. The old writer also reported the requested length when capacity allowed fewer bytes.

The repair introduces `OSFS_BLOCK_SIZE`, bounds extent access by the configured capacity, checks allocation errors, and reports partial writes accurately. Preprocessing the corrected source showed `2 * 4096` for extent addressing.

Recorded output from the repaired module:

```text
PASS basic create/write/read
PASS 10 KiB single write and overwrite across 8 KiB boundary
PASS ten sequential 1 KiB writes
PASS 16 KiB capacity, honest short write, EOF and EFBIG
PASS allocation failure preserves partial write and reports ENOSPC
ALL_LAB4_READBACK_TESTS_PASS
LAB4_FIXED_MODULE_UNLOADED
```

The test compares nonzero contents and checks return counts/errors; checking file size alone is insufficient. No osfs module or mount remained after the run.

The kernel build emitted a compiler-name warning although both reported GCC 13.3.0, and skipped optional BTF generation because vmlinux was unavailable. A later build in the VirtualBox shared folder emitted clock-skew warnings; the runtime-verified module came from a Linux-local build. Prefer local VM storage for future builds.

## Remaining limits

Lab 1's maximum-payload failure is not fixed here. Some matrix outputs accumulate into malloc-allocated storage without explicit zero initialization. The local matrix inputs used by the procfs exercises are not shipped. Lab 4 remains memory-backed and has not been established as safe for concurrent access, sparse files, truncation, or arbitrary fault injection. Neither build success nor these specific runtime passes imply production reliability or an official course score.
