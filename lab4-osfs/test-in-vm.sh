#!/usr/bin/env bash
set -euo pipefail
[[ $EUID -eq 0 ]] || { echo 'Run with sudo bash.'; exit 1; }
review_dir=$(cd -- "$(dirname -- "$0")" && pwd)
cd "$review_dir"
if lsmod | grep -Eq '^osfs '; then
  echo 'osfs is already loaded; stop without changing it.'
  exit 1
fi
if findmnt -rn -t osfs >/dev/null; then
  echo 'osfs is already mounted; stop without changing it.'
  exit 1
fi
mount_dir=$(mktemp -d /tmp/osfs-fixed-mount.XXXXXX)
loaded=0
cleanup() {
  if mountpoint -q "$mount_dir"; then
    umount "$mount_dir" || return
  fi
  if [[ $loaded -eq 1 ]]; then
    rmmod osfs || return
  fi
  rmdir "$mount_dir"
}
trap cleanup EXIT
test -f osfs.ko
insmod ./osfs.ko
loaded=1
mount -t osfs none "$mount_dir"
python3 ./test_readback.py "$mount_dir"
umount "$mount_dir"
rmmod osfs
loaded=0
echo LAB4_FIXED_MODULE_UNLOADED
