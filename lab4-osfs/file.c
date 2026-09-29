#include <linux/fs.h>
#include <linux/uaccess.h>
#include "osfs.h"

/**
 * Function: osfs_read
 * Description: Reads data from a file.
 * Inputs:
 *   - filp: The file pointer representing the file to read from.
 *   - buf: The user-space buffer to copy the data into.
 *   - len: The number of bytes to read.
 *   - ppos: The file position pointer.
 * Returns:
 *   - The number of bytes read on success.
 *   - 0 if the end of the file is reached.
 *   - -EFAULT if copying data to user space fails.
 */
static ssize_t osfs_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
  {
    struct inode *inode = file_inode(filp);
    struct osfs_inode *osfs_inode = inode->i_private;
    struct osfs_sb_info *sb_info = inode->i_sb->s_fs_info;
    void *data_block;
    ssize_t bytes_read;
    loff_t index = *ppos / (MAX_CONTINUE_BLOCKS * OSFS_BLOCK_SIZE), offset = *ppos % (MAX_CONTINUE_BLOCKS * OSFS_BLOCK_SIZE);
    const loff_t capacity = MAX_EXTENTS * MAX_CONTINUE_BLOCKS * OSFS_BLOCK_SIZE;
    if (*ppos < 0)
        return -EINVAL;
    if (!len || *ppos >= capacity)
        return 0;
    len = min_t(size_t, len, capacity - *ppos);
    // If the file has not been allocated a data block, it indicates the file is empty
    if (osfs_inode->extents[0].block_count == 0)
    {
        pr_err("Read: index no block");
        return 0;
    }

    if (*ppos >= osfs_inode->i_size)
    {
        pr_err("Read: overhead");
        return 0;
    }

    if (*ppos + len > osfs_inode->i_size)
        len = osfs_inode->i_size - *ppos;
    
    bytes_read = len;
    int lens[MAX_EXTENTS], lens_index = 1;
    lens[0] = len;
    // Step3: Limit the write length to fit within one data block
    if(offset + len > OSFS_BLOCK_SIZE * MAX_CONTINUE_BLOCKS)
    {
        lens[0] = OSFS_BLOCK_SIZE * MAX_CONTINUE_BLOCKS - offset;
        len -= lens[0];
        while(lens_index + index < MAX_EXTENTS && len > 0)
        {
            lens[lens_index] = min(len, OSFS_BLOCK_SIZE * MAX_CONTINUE_BLOCKS);
            len -= lens[lens_index];
            lens_index++;
        }
    }
    
    data_block = sb_info->data_blocks + osfs_inode->extents[index].start_block * OSFS_BLOCK_SIZE + offset;
    if (copy_to_user(buf, data_block, lens[0]))
        return -EFAULT;
    *ppos += lens[0];
    buf += lens[0];
    for(int index_lens = 1; index_lens < lens_index; index_lens++)
    {
        data_block = sb_info->data_blocks + osfs_inode->extents[index + index_lens].start_block * OSFS_BLOCK_SIZE;
        if (copy_to_user(buf, data_block, lens[index_lens]))
            return -EFAULT;
        *ppos += lens[index_lens];
        buf += lens[index_lens];
    }
    
    return bytes_read;
}


/**
 * Function: osfs_write
 * Description: Writes data to a file.
 * Inputs:
 *   - filp: The file pointer representing the file to write to.
 *   - buf: The user-space buffer containing the data to write.
 *   - len: The number of bytes to write.
 *   - ppos: The file position pointer.
 * Returns:
 *   - The number of bytes written on success.
 *   - -EFAULT if copying data from user space fails.
 *   - Adjusted length if the write exceeds the block size.
 */
static ssize_t osfs_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{   
    //Step1: Retrieve the inode and filesystem information
    struct inode *inode = file_inode(filp);
    struct osfs_inode *osfs_inode = inode->i_private;
    struct osfs_sb_info *sb_info = inode->i_sb->s_fs_info;
    void *data_block;
    ssize_t bytes_written;
    int ret, lens_index = 1;
    loff_t start_pos = *ppos;
    const loff_t capacity = MAX_EXTENTS * MAX_CONTINUE_BLOCKS * OSFS_BLOCK_SIZE;
    loff_t index = *ppos / (MAX_CONTINUE_BLOCKS * OSFS_BLOCK_SIZE), offset = *ppos % (MAX_CONTINUE_BLOCKS * OSFS_BLOCK_SIZE);
    int lens[MAX_EXTENTS];

    if (*ppos < 0)
        return -EINVAL;
    if (!len)
        return 0;
    if (*ppos >= capacity)
        return -EFBIG;
    /* Report a short write rather than claiming bytes beyond capacity. */
    len = min_t(size_t, len, capacity - *ppos);
    
    if(osfs_inode->extents[index].block_count == 0){
        ret = osfs_alloc_data_block(sb_info, &osfs_inode->extents[index].start_block);
        if (ret)
            return ret;
        osfs_inode->extents[index].block_count = MAX_CONTINUE_BLOCKS;
        osfs_inode->extent_count++;
    }
    
    bytes_written = len;
    lens[0] = len;
    // Step3: Limit the write length to fit within one data block
    if(offset + len > OSFS_BLOCK_SIZE * MAX_CONTINUE_BLOCKS)
    {
        lens[0] = OSFS_BLOCK_SIZE * MAX_CONTINUE_BLOCKS - offset;
        len -= lens[0];
        while(lens_index + index < MAX_EXTENTS && len > 0)
        {
            lens[lens_index] = min(len, OSFS_BLOCK_SIZE * MAX_CONTINUE_BLOCKS);
            len -= lens[lens_index];
            lens_index++;
        }
    }
    
    data_block = sb_info->data_blocks + osfs_inode->extents[index].start_block * OSFS_BLOCK_SIZE + offset;
    if(copy_from_user(data_block, buf, lens[0])) return -EFAULT;
    *ppos += lens[0];
    buf += lens[0];
    
    if(1 == lens_index)
    {
        osfs_inode->extents[index].file_offset += lens[0];
    }
    else
    {
        osfs_inode->extents[index].file_offset = OSFS_BLOCK_SIZE * MAX_CONTINUE_BLOCKS;
    }
    
    for(int index_lens = 1; index_lens < lens_index; index_lens++)
    {
        if(osfs_inode->extents[index + index_lens].block_count == 0){
            ret = osfs_alloc_data_block(sb_info, &osfs_inode->extents[index + index_lens].start_block);
            if (ret) {
                bytes_written = *ppos - start_pos;
                goto update_metadata;
            }
            osfs_inode->extents[index + index_lens].block_count = MAX_CONTINUE_BLOCKS;
            osfs_inode->extent_count++;
        }
        // Step4: Write data from user space to the data block
        data_block = sb_info->data_blocks + osfs_inode->extents[index + index_lens].start_block * OSFS_BLOCK_SIZE;
        if(copy_from_user(data_block, buf, lens[index_lens])) {
            bytes_written = *ppos - start_pos;
            goto update_metadata;
        }
        *ppos += lens[index_lens];
        buf += lens[index_lens];
        
        if(index_lens + 1 == lens_index)
        {
            osfs_inode->extents[index + index_lens].file_offset += lens[index_lens];
        }
        else
        {
            osfs_inode->extents[index + index_lens].file_offset = OSFS_BLOCK_SIZE * MAX_CONTINUE_BLOCKS;
        }
    }
    
update_metadata:
    // 1. 使用核心 API 設定 VFS inode 的時間
    struct timespec64 now = current_time(inode);
    inode_set_mtime_to_ts(inode, now);
    inode_set_ctime_to_ts(inode, now);

    // 2. 同步更新你的 osfs_inode 內部時間數據（這是你的檔案系統私有數據）
    osfs_inode->__i_mtime = now;
    osfs_inode->__i_ctime = now;

    // 3. 標記 inode 為 dirty 以確保變更被記錄
    mark_inode_dirty(inode);
    if(*ppos > osfs_inode->i_size){
        osfs_inode->i_size = *ppos;
        inode->i_size = osfs_inode->i_size;
    }
    return bytes_written;
}

/**
 * Struct: osfs_file_operations
 * Description: Defines the file operations for regular files in osfs.
 */
const struct file_operations osfs_file_operations = {
    .open = generic_file_open, // Use generic open or implement osfs_open if needed
    .read = osfs_read,
    .write = osfs_write,
    .llseek = default_llseek,
    // Add other operations as needed
};

/**
 * Struct: osfs_file_inode_operations
 * Description: Defines the inode operations for regular files in osfs.
 * Note: Add additional operations such as getattr as needed.
 */
const struct inode_operations osfs_file_inode_operations = {
    // Add inode operations here, e.g., .getattr = osfs_getattr,
};
