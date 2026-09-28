#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "3_2_Config.h"

#define matrix_row_x 1234
#define matrix_col_x 250
#define matrix_row_y 250
#define matrix_col_y 1234

int **x;
int **y;
int **z;

FILE *fptr1;
FILE *fptr2;
FILE *fptr3;

/* ================= matrix input ================= */
void data_processing(void){
    int tmp;
    fscanf(fptr1, "%d", &tmp);
    fscanf(fptr1, "%d", &tmp);
    for(int i=0;i<matrix_row_x;i++)
        for(int j=0;j<matrix_col_x;j++)
            fscanf(fptr1, "%d", &x[i][j]);

    fscanf(fptr2, "%d", &tmp);
    fscanf(fptr2, "%d", &tmp);
    for(int i=0;i<matrix_row_y;i++)
        for(int j=0;j<matrix_col_y;j++)
            fscanf(fptr2, "%d", &y[i][j]);
}

/* ================= thread 1 ================= */
void *thread1(void *arg){
    char msg[] = "Thread 1 says hello!";

#if (THREAD_NUMBER == 1)
    for(int i=0;i<matrix_row_x;i++)
        for(int j=0;j<matrix_col_y;j++)
            for(int k=0;k<matrix_row_y;k++)
                z[i][j] += x[i][k] * y[k][j];
#else
    for(int i=0;i<matrix_row_x/2;i++)
        for(int j=0;j<matrix_col_y;j++)
            for(int k=0;k<matrix_row_y;k++)
                z[i][j] += x[i][k] * y[k][j];
#endif

    /* ===== write to proc ===== */
    int fd = open("/proc/mythread_info", O_WRONLY);
    write(fd, msg, strlen(msg));
    close(fd);

    /* ===== read from proc (read once) ===== */
    FILE *fp = fopen("/proc/mythread_info", "r");
    char buf[256];
    for(int i=0;i<4;i++){
        if(fgets(buf, sizeof(buf), fp))
            printf("%s", buf);
    }
    fclose(fp);

    return NULL;
}

#if (THREAD_NUMBER == 2)
/* ================= thread 2 ================= */
void *thread2(void *arg){
    char msg[] = "Thread 2 says hello!";

    for(int i=matrix_row_x/2;i<matrix_row_x;i++)
        for(int j=0;j<matrix_col_y;j++)
            for(int k=0;k<matrix_row_y;k++)
                z[i][j] += x[i][k] * y[k][j];

    /* ===== write to proc ===== */
    int fd = open("/proc/mythread_info", O_WRONLY);
    write(fd, msg, strlen(msg));
    close(fd);

    /* ===== read from proc ===== */
    FILE *fp = fopen("/proc/mythread_info", "r");
    char buf[256];
    for(int i=0;i<4;i++){
        if(fgets(buf, sizeof(buf), fp))
            printf("%s", buf);
    }
    fclose(fp);

    return NULL;
}
#endif

/* ================= main ================= */
int main(){
    x = malloc(sizeof(int*) * matrix_row_x);
    for(int i=0;i<matrix_row_x;i++)
        x[i] = malloc(sizeof(int) * matrix_col_x);

    y = malloc(sizeof(int*) * matrix_row_y);
    for(int i=0;i<matrix_row_y;i++)
        y[i] = malloc(sizeof(int) * matrix_col_y);

    z = malloc(sizeof(int*) * matrix_row_x);
    for(int i=0;i<matrix_row_x;i++)
        z[i] = calloc(matrix_col_y, sizeof(int));

    fptr1 = fopen("m1.txt","r");
    fptr2 = fopen("m2.txt","r");
    fptr3 = fopen("3_2.txt","a");

    data_processing();
    fprintf(fptr3, "%d %d\n", matrix_row_x, matrix_col_y);

    pthread_t t1, t2;
    pthread_create(&t1, NULL, thread1, NULL);
#if (THREAD_NUMBER == 2)
    pthread_create(&t2, NULL, thread2, NULL);
#endif

    pthread_join(t1, NULL);
#if (THREAD_NUMBER == 2)
    pthread_join(t2, NULL);
#endif

    for(int i=0;i<matrix_row_x;i++){
        for(int j=0;j<matrix_col_y;j++){
            fprintf(fptr3, "%d ", z[i][j]);
            if(j==matrix_col_y-1) fprintf(fptr3, "\n");
        }
    }

    fclose(fptr1);
    fclose(fptr2);
    fclose(fptr3);
    return 0;
}
