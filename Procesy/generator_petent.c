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

#define ILE_SEMAFOROW 9
#define SEMAFOR_DYREKTOR 1
#define SEMAFOR_GENERATOR 2

void SIGUSR1_handle(int sig);
void SIGUSR2_handle(int sig);

int recieve_dyrektor(int sems, key_t *shared_mem, int result[]);

int main(int argc, char **argv)
{
    signal(SIGUSR1, SIGUSR1_handle);
    signal(SIGUSR2, SIGUSR2_handle);

    key_t key;
    key = atoi(argv[1]);

    int sems = semget(key, ILE_SEMAFOROW, 0); // semafory
    if (sems == -1)
    {
        perror("generator semget");
        exit(1);
    }

    int shm_id = shmget(key, sizeof(key_t), 0); // pamiec
    if (shm_id == -1)
    {
        perror("generator shmget");
        exit(1);
    }

    key_t *shared_mem = (key_t *)shmat(shm_id, NULL, 0); // podlaczamy pamiec
    if (shared_mem == (key_t *)-1)
    {
        perror("generator shmat");
        exit(1);
    }

    key_t tab[2];
    if (recieve_dyrektor(sems, shared_mem, tab) != 0)
    {
        perror("generator recieve dyrektor");
        exit(1); // TODO: tu jest problem, nie ma mechanizmu do wykraczania jeśli podproces się wykraczy
    }
    sleep(10);
    return 0;
}

void SIGUSR1_handle(int sig)
{
    printf("generator przychwycil SIGUSR1\n");
}

void SIGUSR2_handle(int sig)
{
    printf("generator przychwycil SIGUSR2\n");
}

int recieve_dyrektor(int sems, key_t *shared_mem, int result[])
{
    struct sembuf P = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_GENERATOR, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < 2; i++)
    {
        while (semop(sems, &P, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("dyrektor semop P");
                return 1;
            }
        }

        result[i] = *shared_mem; // TODO: przesyłanie int a odbieranie key_t jest głupie

        while (semop(sems, &V, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("dyrektor semop V");
                return 1;
            }
        }
    }
    return 0;
}