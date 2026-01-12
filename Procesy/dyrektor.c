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

// TODO: cleanup(), w normalnym workflow to on bedzie usuwal semafory
// TODO: wysyłanie sygnałów do procesów

int main(int argc, char **argv)
{
    printf("dyrektor\n");
    return 0;
}