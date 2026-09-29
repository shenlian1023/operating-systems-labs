# OSFS: extent-backed files in memory

[Project overview](../README.md) · [Implementation](../docs/implementation.md#translating-a-file-position-into-an-extent-and-offset) · [Validation record](../docs/validation-2026-09-29.md)

Two extents hold two 4 KiB blocks each, giving each regular file 16 KiB of configured capacity. The module implements file creation and segmented reads/writes through Linux VFS. Contents disappear when the filesystem is unmounted; this is not durable storage.

## Build and test

Use a disposable Linux VM with headers matching `uname -r`. Kernel bugs can crash the VM; save work first. Build on the VM's local Linux filesystem rather than a VirtualBox shared folder, where timestamp differences can trigger make's clock-skew warnings.

From this directory:

```bash
test -d "/lib/modules/$(uname -r)/build"
make
sudo bash ./test-in-vm.sh
```

The runner loads the module, creates a fresh mount, runs `test_readback.py`, then unmounts and unloads on success or failure. It refuses to run if another `osfs` instance is already loaded or mounted. If cleanup itself fails, inspect `findmnt -t osfs` and `lsmod` before retrying.

## What the regression test checks

| Check | Assertion |
| --- | --- |
| Basic creation and I/O | Written text and file size match the readback |
| Cross-extent I/O | A 10 KiB pattern and a boundary-crossing overwrite read back correctly |
| Sequential writes | Ten 1 KiB writes reproduce the original pattern |
| Capacity boundary | A 16 KiB file is readable; oversized requests return short counts; another byte returns `EFBIG` |
| Allocation exhaustion | Completed bytes remain readable; a retry without free blocks returns `ENOSPC` |

Success ends with `ALL_LAB4_READBACK_TESTS_PASS` and `LAB4_FIXED_MODULE_UNLOADED`.

The 2026-09-29 run passed on Linux `6.14.0-37-generic`. Compiler-name and missing-vmlinux BTF warnings did not prevent that build or runtime check. No prebuilt `.ko` is committed: build against the kernel where you will load it.

## Scope

The follow-up repair fixes the block-size macro collision and the tested capacity/allocation-error paths. These checks do not establish concurrency safety, disk persistence, sparse-file behavior, truncation support, or complete recovery from user-copy faults. The original course test notes remain under [docs/original-test-notes.md](docs/original-test-notes.md).
