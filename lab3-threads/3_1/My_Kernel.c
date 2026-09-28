#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <asm/current.h>

#define procfs_name "Mythread_info"
#define BUFSIZE  1024
char buf[BUFSIZE];

static ssize_t Mywrite(struct file *fileptr, const char __user *ubuf, size_t buffer_len, loff_t *offset){
    /* Do nothing */
	return 0;
}

static ssize_t Myread(struct file *fileptr, char __user *ubuf, size_t buffer_len, loff_t *offset){
    /*Your code here*/
    char buf[1024]; 
    int len = 0;
    struct task_struct *thread;
    
    // 修正變數名稱：將 ppos 改為 offset，count 改為 buffer_len 
    if (*offset > 0 || buffer_len < 1024) return 0;

    // 使用 for_each_thread 走訪所有執行緒 [cite: 364]
    for_each_thread(current, thread) {
        // 修正 sprintf 語法與格式化字元 %u [cite: 351, 357]
        if (thread->pid != current->tgid) {
        len += sprintf(buf + len, "PID: %d, TID: %d, Priority: %d, State: %u\n", 
                       current->tgid,    // 行程 ID [cite: 363]
                       thread->pid,      // 執行緒 ID [cite: 365]
                       thread->prio,     // 優先權 [cite: 366]
                       (unsigned int)thread->__state); // 執行緒狀態 [cite: 367]
        }
    }

    // 將資料拷貝到使用者空間 [cite: 277, 293]
    if (copy_to_user(ubuf, buf, len)) {
        return -EFAULT;
    }

    *offset += len; // 更新偏移量 [cite: 283]
    return len;     // 回傳讀取長度 [cite: 283]
    /****************/
}

static struct proc_ops Myops = {
    .proc_read = Myread,
    .proc_write = Mywrite,
};

static int My_Kernel_Init(void){
    proc_create(procfs_name, 0644, NULL, &Myops);   
    pr_info("My kernel says Hi");
    return 0;
}

static void My_Kernel_Exit(void){
    pr_info("My kernel says GOODBYE");
}

module_init(My_Kernel_Init);
module_exit(My_Kernel_Exit);

MODULE_LICENSE("GPL");