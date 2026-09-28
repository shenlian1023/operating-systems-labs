# unload mod
# 1. 確保不在 mnt 目錄內
cd ~/OS2025/os_2025_lab4/

# 2. 解除掛載檔案系統
sudo umount mnt

# 3. 移除舊的核心模組 (這是解決 File exists 錯誤的關鍵)
sudo rmmod osfs

# 4. 確認模組已移除 (若沒看到 osfs 代表成功)
lsmod | grep osfs
------------------------------------------------------------------
# makefile
# 1. 清理並重新編譯
make clean
make

# 2. 載入修正後的 Bonus 版本模組
sudo insmod osfs.ko

# 3. 建立掛載點並掛載
mkdir -p mnt
sudo mount -t osfs none mnt/
cd mnt
--------------------------------------------------------------------
# 測試檔案建立與寫入 (Requirement 1 & 2)
sudo touch test1.txt
sudo bash -c "echo 'I LOVE OSLAB' > test1.txt"
cat test1.txt

# 測試大檔案寫入 (驗證 Bonus - Extent-based Allocation)
# 寫入 10KB (超過單一 4KB 區塊)，測試 Extent 分配邏輯
sudo dd if=/dev/zero of=largefile bs=1024 count=10
ls -lh largefile