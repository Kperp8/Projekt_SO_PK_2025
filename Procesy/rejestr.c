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

void SIGUSR1_handle(int sig);
void SIGUSR2_handle(int sig);

int main(int argc, char **argv)
{
    signal(SIGUSR1, SIGUSR1_handle);
    signal(SIGUSR2, SIGUSR2_handle);
    printf("rejestr\n");
    sleep(10);
    return 0;
}

void SIGUSR1_handle(int sig)
{
    printf("rejestr przychwycil SIGUSR1\n");
}

void SIGUSR2_handle(int sig)
{
    printf("rejestr przychwycil SIGUSR2\n");
}