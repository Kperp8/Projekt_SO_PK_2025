#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

int tab_X[5] = {
    10, // X1
    10, // X2
    10, // X3
    10, // X4
    10, // X5
};

void SIGUSR1_handle(int sig);
void SIGUSR2_handle(int sig);

int main(int argc, char **argv)
{
    signal(SIGUSR1, SIGUSR1_handle);
    signal(SIGUSR2, SIGUSR2_handle);
    printf("urzednik\n");
    sleep(10);
    return 0;
}

void SIGUSR1_handle(int sig)
{
    printf("urzednik przychwycil SIGUSR1\n");
}

void SIGUSR2_handle(int sig)
{
    printf("urzednik przychwycil SIGUSR2\n");
}