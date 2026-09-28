#include "receiver.h"

#define SEM_SENDER_NAME   "/lab1_sender_sem"
#define SEM_RECEIVER_NAME "/lab1_receiver_sem"
#define MQ_NAME           "/lab1_ipc_mq"
#define SHM_NAME          "/lab1_ipc_shm"
#define SHM_SIZE          (sizeof(size_t) + 1024)

static sem_t *g_sem_sender   = NULL;
static sem_t *g_sem_receiver = NULL;
static mqd_t  g_mq           = (mqd_t)-1; //message queue descriptor，一種特殊的int
static int    g_shm_fd       = -1;
static void  *g_shm_ptr      = NULL;

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void mailbox_init(mailbox_t *mb, int flag) {
    mb->flag = flag;

    // 這裡的open就不加O_CREAT，因為receiver要等sender先建立好
    g_sem_sender   = sem_open(SEM_SENDER_NAME, 0); //0是開啟的flag 表示不用創建
    if (g_sem_sender == SEM_FAILED) die("sem_open sender");
    g_sem_receiver = sem_open(SEM_RECEIVER_NAME, 0);
    if (g_sem_receiver == SEM_FAILED) die("sem_open receiver");

    if (flag == MSG_PASSING) {
        g_mq = mq_open(MQ_NAME, O_RDWR); //也不加O_CREAT
        if (g_mq == (mqd_t)-1) die("mq_open");
    } else if (flag == SHARED_MEM) {
        g_shm_fd = shm_open(SHM_NAME, O_RDWR, 0600); //也不加O_CREAT
        if (g_shm_fd < 0) die("shm_open");
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
    (void)mb;

    if (g_mq != (mqd_t)-1) mq_close(g_mq);
    if (g_shm_ptr && g_shm_ptr != MAP_FAILED) munmap(g_shm_ptr, SHM_SIZE);
    if (g_shm_fd >= 0) close(g_shm_fd);
    if (g_sem_sender && g_sem_sender != SEM_FAILED) sem_close(g_sem_sender);
    if (g_sem_receiver && g_sem_receiver != SEM_FAILED) sem_close(g_sem_receiver);
}
//前面都跟sender.c差不多
void receive(message_t* message_ptr, mailbox_t* mailbox_ptr){
    if (!mailbox_ptr || !message_ptr) die("receive: invalid args");

    if (mailbox_ptr->flag == MSG_PASSING) {
        ssize_t n = mq_receive(g_mq, message_ptr->text, sizeof(message_ptr->text), NULL); //取出訊息放到message_ptr->text，最大接收text size
        if (n < 0) die("mq_receive");
        message_ptr->length = (size_t)n;//存取實際接收的長度
    } else if (mailbox_ptr->flag == SHARED_MEM) {
        size_t *len_ptr = (size_t *)g_shm_ptr;//存放字串長度的地方的pointer
        char   *buf_ptr = (char *)g_shm_ptr + sizeof(size_t);//存字串本體的起始pointer
        message_ptr->length = *len_ptr;//讀取字串長度
        memcpy(message_ptr->text, buf_ptr, message_ptr->length);//從共享記憶體複製字串本體到message_ptr->text
        message_ptr->text[message_ptr->length - 1] = '\0';//尾端加個\0保險用
    } else {
        die("receive: invalid mailbox_ptr->flag");
    }
}

int main(int argc, char **argv){
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <1|2>\n  1 = Message Passing (POSIX mq)\n  2 = Shared Memory (POSIX shm)\n", argv[0]);
        return EXIT_FAILURE;
    }

    int flag = atoi(argv[1]);
    mailbox_t mailbox;
    mailbox_init(&mailbox, flag);

    message_t msg;
    double total_recv_seconds = 0.0;
    struct timespec t0, t1;

    while (1) {
        // 等待 sender 解鎖
        if (sem_wait(g_sem_receiver) < 0) die("sem_wait receiver");

        // 計時開始
        clock_gettime(CLOCK_MONOTONIC, &t0);
        receive(&msg, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        total_recv_seconds += (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;

        // output 接收的訊息內容
        printf("Receiving message: %s\n", msg.text);
        fflush(stdout);

        // 若收到 EXIT 訊息則結束
        if (strcmp(msg.text, "__EXIT__") == 0 || strcmp(msg.text, "EOF") == 0) {
            printf("Sender exit!\n");
            break;
        }

        // 若是一般訊息則讓 sender 下一輪繼續
        if (sem_post(g_sem_sender) < 0) die("sem_post sender");
    }

    // output總接收時間
    printf("Total time taken in receiving msg: %.6f s\n", total_recv_seconds);

    mailbox_cleanup(&mailbox);
    return 0;
}
