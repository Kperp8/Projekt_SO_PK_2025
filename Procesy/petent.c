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

// TODO: obsługa sygnałów SIGINT, 1, 2

int main(int argc, char **argv)
{
    printf("%s %s %d pid %d\n", argv[2], argv[3], atoi(argv[4]), getpid());
    return 0;
}