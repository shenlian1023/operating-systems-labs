#include "sender.h"
//define IPC(行程間通訊)物件名稱
#define SEM_SENDER_NAME   "/lab1_sender_sem"
#define SEM_RECEIVER_NAME "/lab1_receiver_sem"
#define MQ_NAME           "/lab1_ipc_mq"
#define SHM_NAME          "/lab1_ipc_shm"
#define SHM_SIZE          (sizeof(size_t) + 1024)//size_t是存字串的長度，1024是存字串本體

static sem_t *g_sem_sender   = NULL; // sender semaphore
static sem_t *g_sem_receiver = NULL; // receiver semaphore 是放在kernel裡面的物件，所以才要用pointer
static mqd_t  g_mq           = (mqd_t)-1;// message queue，(mqd_t)-1還沒開啟或failed
static int    g_shm_fd       = -1; // shared memory file descriptor， -1還沒開啟或failed
static void  *g_shm_ptr      = NULL; // mapped address，指向shared memory的起始位置

static void die(const char *msg) {
    perror(msg); //錯誤輸出的函式
    exit(EXIT_FAILURE); //EXIT_FAILURE(不正常的離開) 是 <stdlib.h> 定義的常數
}

static void trim_newline(char *s) {//去掉字串最後的換行符號，乾淨一點
    size_t n = strlen(s);
    if (n && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
    if (n && (s[n-1] == '\r')) s[--n] = '\0';
}

static void mailbox_init(mailbox_t *mb, int flag) {
    mb->flag = flag;//把flag存到mailbox struct裡面

    // 建立 semaphore：sender 先啟動 (1)，receiver 等待 (0)
    g_sem_sender   = sem_open(SEM_SENDER_NAME,   O_CREAT, 0600, 1);//O_CREAT表示如果不存在就建立，0600是權限(owner可讀寫)，1是初始值
    if (g_sem_sender == SEM_FAILED) die("sem_open sender");
    g_sem_receiver = sem_open(SEM_RECEIVER_NAME, O_CREAT, 0600, 0);
    if (g_sem_receiver == SEM_FAILED) die("sem_open receiver");

    if (flag == MSG_PASSING) {
        struct mq_attr attr = {0};//初始化為0
        attr.mq_maxmsg  = 10; //最大訊息數量
        attr.mq_msgsize = 1024; //每則訊息的最大長度
        g_mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0600, &attr); //O_CREAT表示如果不存在就建立，O_RDWR表示可讀寫
        if (g_mq == (mqd_t)-1) die("mq_open");
    } else if (flag == SHARED_MEM) {
        g_shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600); //O_CREAT表示如果不存在就建立，O_RDWR表示可讀寫
        if (g_shm_fd < 0) die("shm_open");
        if (ftruncate(g_shm_fd, SHM_SIZE) < 0) die("ftruncate"); //設定shared memory大小
        g_shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, g_shm_fd, 0); //映射共享記憶體
        if (g_shm_ptr == MAP_FAILED) die("mmap");
    } else {
        fprintf(stderr, "Invalid flag: %d (1=Message Passing, 2=Shared Memory)\n", flag);
        exit(EXIT_FAILURE);
    }

    // title output
    if (flag == MSG_PASSING)
        printf("Message Passing\n");
    else
        printf("Shared Memory\n");
}

static void mailbox_cleanup(mailbox_t *mb) {
    (void)mb;// 清除warning
    // 關閉 IPC 物件
    if (g_mq != (mqd_t)-1) mq_close(g_mq);
    if (g_shm_ptr && g_shm_ptr != MAP_FAILED) munmap(g_shm_ptr, SHM_SIZE);
    if (g_shm_fd >= 0) close(g_shm_fd);
    if (g_sem_sender && g_sem_sender != SEM_FAILED) sem_close(g_sem_sender);
    if (g_sem_receiver && g_sem_receiver != SEM_FAILED) sem_close(g_sem_receiver);//close只是關閉連接
    // 清空所有 IPC 物件的檔案
    sem_unlink(SEM_SENDER_NAME);
    sem_unlink(SEM_RECEIVER_NAME);
    mq_unlink(MQ_NAME);
    shm_unlink(SHM_NAME);

}

void send(message_t message, mailbox_t* mailbox_ptr){
    if (!mailbox_ptr) die("send: mailbox_ptr NULL");

    if (mailbox_ptr->flag == MSG_PASSING) {
        if (mq_send(g_mq, message.text, message.length, 0) < 0)//滿了會回傳-1
            die("mq_send");
    } else if (mailbox_ptr->flag == SHARED_MEM) {
        size_t *len_ptr = (size_t *)g_shm_ptr; //shared memory的前面sizeof(size_t) bytes是用來存字串長度的
        char   *buf_ptr = (char *)g_shm_ptr + sizeof(size_t); //shared memory的後面1024 bytes是用來存字串本體的
        *len_ptr = message.length; //存字串長度
        memcpy(buf_ptr, message.text, message.length); //從message.text複製message.length bytes到buf_ptr(共享記憶體)
    } else {
        die("send: invalid mailbox_ptr->flag");
    }
}

int main(int argc, char **argv){
    if (argc < 3) { // 檢查參數數量，確定有提供模式和input.txt
        fprintf(stderr, "Usage: %s <1|2> <input.txt>\n  1 = Message Passing (POSIX mq)\n  2 = Shared Memory (POSIX shm)\n", argv[0]);
        return EXIT_FAILURE;
    }

    int flag = atoi(argv[1]);
    const char *input_path = argv[2];

    mailbox_t mailbox;
    mailbox_init(&mailbox, flag);//初始化mailbox

    FILE *fp = fopen(input_path, "r"); //打開input.txt
    if (!fp) die("fopen input");

    char line[1050];
    double total_send_seconds = 0.0;
    struct timespec t0, t1;//from <time.h>

    while (fgets(line, sizeof(line), fp)) {//從input.txt讀一行
        trim_newline(line); //去掉換行符號
        if (line[0] == '\0') continue;

        int is_exit_line = (strcmp(line, "EOF") == 0);//檢查是否為EOF行

        message_t msg;
        size_t len = strnlen(line, 1024);//計算字串長度，最多1024
        if (len > 1024) len = 1024;//避免超過1024
        memcpy(msg.text, line, len);//把line複製到msg.text，但是不會複製結束符號\0
        msg.text[len] = '\0';//加上結束符號，也避免強制截斷len後面沒有結束符號
        msg.length = len + 1;//length包含結束符號

        if (sem_wait(g_sem_sender) < 0) die("sem_wait sender"); // 等待 receiver 結束後解鎖 sender

        // output 傳送的訊息內容
        printf("Sending message: %s\n", line);
        fflush(stdout);

        clock_gettime(CLOCK_MONOTONIC, &t0);
        send(msg, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        total_send_seconds += (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;//累加傳送時間

        if (sem_post(g_sem_receiver) < 0) die("sem_post receiver"); // 解鎖 receiver

        if (is_exit_line) {//收到EOF行就結束
            printf("End of input file! exit!\n");
            goto done;
        }
    }

    // 若無 EOF 行，補送 __EXIT__
    {
        message_t bye;
        const char *EXIT_STR = "__EXIT__";//定義結束字串
        size_t len = strnlen(EXIT_STR, 1024);//計算長度
        memcpy(bye.text, EXIT_STR, len);//複製到bye.text
        bye.text[len] = '\0';//加上結束符號
        bye.length = len + 1;//length包含結束符號

        if (sem_wait(g_sem_sender) < 0) die("sem_wait sender (exit)");
        printf("End of input file! exit!\n");
        
        fflush(stdout);

        clock_gettime(CLOCK_MONOTONIC, &t0);
        send(bye, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        total_send_seconds += (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;

        if (sem_post(g_sem_receiver) < 0) die("sem_post receiver (exit)");
    }

done:
    fclose(fp);//關閉input.txt

    // output總傳送時間
    printf("Total time taken in sending msg: %.6f s\n", total_send_seconds);

    mailbox_cleanup(&mailbox);//清除IPC物件
    return 0;
}
