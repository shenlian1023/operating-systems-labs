#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/*Note: Value of LOCK is 0 and value of UNLOCK is 1.*/
#define LOCK 0
#define UNLOCK 1

volatile int a = 0;
volatile int lock = UNLOCK;
pthread_mutex_t mutex;

void spin_lock() {
    asm volatile(
        "loop:\n\t"
        "mov $0, %%eax\n\t"      // 將 0 (LOCK) 放進 eax 暫存器
        /*YOUR CODE HERE*/
        "xchg %%eax, %[lock]\n\t" // 原子性交換 eax 與 lock 變數的值 [cite: 118, 125]
        /****************/
        "cmp $0, %%eax\n\t"       // 檢查 eax (舊的 lock 值) 是否為 0
        "je loop\n\t"             // 如果舊值是 0 (原本就是鎖住的)，跳回 loop 繼續旋轉
        :
        : [lock] "m" (lock)
        : "eax", "memory"
    );
}

void spin_unlock() {
    asm volatile(
        "mov $1, %%eax\n\t"      // 將 1 (UNLOCK) 放進 eax [cite: 163]
        /*YOUR CODE HERE*/
        "xchg %%eax, %[lock]\n\t" // 將 lock 的值改回 1 [cite: 118]
        /****************/
        :
        : [lock] "m" (lock)
        : "eax", "memory"
    );
}


void *thread(void *arg) {

    for(int i=0; i<10000; i++){

        spin_lock();
        a = a + 1;
        spin_unlock();
    }
    return NULL;
}

int main() {
    FILE *fptr;
    fptr = fopen("1.txt", "a");
    pthread_t t1, t2;

    pthread_mutex_init(&mutex, 0);
    pthread_create(&t1, NULL, thread, NULL);
    pthread_create(&t2, NULL, thread, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_mutex_destroy(&mutex);

    fprintf(fptr, "%d ", a);
    fclose(fptr);
}

