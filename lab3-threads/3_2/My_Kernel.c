#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>   // copy_to_user, copy_from_user
#include <linux/string.h>    // strlen, memset
#include <linux/types.h>     // u64
#include <asm/current.h>

#define PROC_NAME "mythread_info"
#define BUFSIZE 1024

static char buf[BUFSIZE];        // 存使用者寫入字串
static char out_buf[BUFSIZE];    // read 用輸出 buffer
static struct proc_dir_entry *pde;

/* ===== 1) forward declarations：先宣告 ===== */
static ssize_t Myread(struct file *fileptr,
                      char __user *ubuf,
                      size_t buffer_len,
                      loff_t *offset);

static ssize_t Mywrite(struct file *fileptr,
                       const char __user *ubuf,
                       size_t buffer_len,
                       loff_t *offset);

/* ===== 2) proc_ops：宣告完函式後，才可以指到它們 ===== */
static const struct proc_ops Myops = {
    .proc_read  = Myread,
    .proc_write = Mywrite,
};

/* ===== 3) Myread 定義 ===== */
static ssize_t Myread(struct file *fileptr,
                      char __user *ubuf,
                      size_t buffer_len,
                      loff_t *offset)
{
    int len;
    u64 time_ms;

    if (!ubuf) return -EINVAL;

    // 防止 cat 無限讀：第二次進來直接回 0
    if (*offset > 0)
        return 0;

    // spec: current->utime/100/1000
    time_ms = current->utime / 100 / 1000;

    len = snprintf(out_buf, BUFSIZE,
    "%s\n"
    "PID: %d, TID: %d, time: %llu\n",
    buf[0] ? buf : "(empty)",
    current->tgid,
    current->pid,
    (unsigned long long)time_ms);


    if (len < 0) return -EINVAL;
    if (len > buffer_len) len = buffer_len;

    if (copy_to_user(ubuf, out_buf, len))
        return -EFAULT;

    *offset += len;
    return len;
}

/* ===== 4) Mywrite 定義 ===== */
static ssize_t Mywrite(struct file *fileptr,
                       const char __user *ubuf,
                       size_t buffer_len,
                       loff_t *offset)
{
    size_t n;

    if (!ubuf) return -EINVAL;

    n = (buffer_len >= BUFSIZE) ? (BUFSIZE - 1) : buffer_len;

    if (copy_from_user(buf, ubuf, n))
        return -EFAULT;

    buf[n] = '\0';

    // 去掉 echo 附帶的 '\n'
    if (n > 0 && buf[n - 1] == '\n')
        buf[n - 1] = '\0';

    return buffer_len;
}

/* ===== 5) init/exit ===== */
static int __init My_Kernel_Init(void)
{
    memset(buf, 0, sizeof(buf));
    pde = proc_create(PROC_NAME, 0644, NULL, &Myops);
    if (!pde) {
        pr_err("proc_create failed\n");
        return -ENOMEM;
    }
    pr_info("My kernel says Hi\n");
    return 0;
}

static void __exit My_Kernel_Exit(void)
{
    if (pde)
        remove_proc_entry(PROC_NAME, NULL);
    pr_info("My kernel says GOODBYE\n");
}

module_init(My_Kernel_Init);
module_exit(My_Kernel_Exit);

MODULE_LICENSE("GPL");
