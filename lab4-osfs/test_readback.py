"""Exercise the mounted OSFS using real read/write syscalls and literal bounds."""
import errno
import os
import sys
from pathlib import Path

root = Path(sys.argv[1])
pattern = bytes(range(256)) * 40


def readback(name, expected):
    path = root / name
    assert path.stat().st_size == len(expected), (name, path.stat().st_size, len(expected))
    assert path.read_bytes() == expected, f'{name}: readback differs'


def create(name):
    return os.open(root / name, os.O_CREAT | os.O_EXCL | os.O_RDWR, 0o600)


fd = create('basic.txt')
assert os.write(fd, b'I LOVE OSLAB\n') == 13
os.close(fd)
readback('basic.txt', b'I LOVE OSLAB\n')
print('PASS basic create/write/read', flush=True)

fd = create('single-10k.bin')
assert os.write(fd, pattern) == 10240
expected = bytearray(pattern)
assert os.pwrite(fd, b'extent-boundary!', 8190) == 16
expected[8190:8206] = b'extent-boundary!'
os.close(fd)
readback('single-10k.bin', bytes(expected))
print('PASS 10 KiB single write and overwrite across 8 KiB boundary', flush=True)

fd = create('chunked-10k.bin')
for offset in range(0, 10240, 1024):
    assert os.write(fd, pattern[offset:offset + 1024]) == 1024
os.close(fd)
readback('chunked-10k.bin', pattern)
print('PASS ten sequential 1 KiB writes', flush=True)

fd = create('capacity.bin')
capacity_payload = bytes(range(256)) * 64
assert os.write(fd, capacity_payload) == 16384
assert os.read(fd, 1) == b''
try:
    os.write(fd, b'x')
except OSError as exc:
    assert exc.errno == errno.EFBIG, exc
else:
    raise AssertionError('Write beyond 16 KiB must fail with EFBIG')
os.lseek(fd, 0, os.SEEK_SET)
assert os.write(fd, bytes(range(256)) * 80) == 16384
assert os.lseek(fd, 0, os.SEEK_CUR) == 16384
assert os.write(fd, b'') == 0
os.close(fd)
readback('capacity.bin', capacity_payload)
print('PASS 16 KiB capacity, honest short write, EOF and EFBIG', flush=True)

# Twenty blocks: root=2, basic=2, two 10 KiB files=8, capacity=4.
# Two new files reserve the remaining four blocks before the last write.
fd = create('reserve.bin')
os.close(fd)
fd = create('no-space.bin')
assert os.write(fd, pattern) == 8192
try:
    os.write(fd, pattern[8192:])
except OSError as exc:
    assert exc.errno == errno.ENOSPC, exc
else:
    raise AssertionError('Exhausted allocation must fail with ENOSPC')
os.close(fd)
readback('no-space.bin', pattern[:8192])
print('PASS allocation failure preserves partial write and reports ENOSPC', flush=True)
print('ALL_LAB4_READBACK_TESTS_PASS', flush=True)
