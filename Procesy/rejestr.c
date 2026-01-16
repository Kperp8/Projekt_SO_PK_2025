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

#define SEMAFOR_DYREKTOR 1
#define SEMAFOR_REJESTR 3
#define ILE_SEMAFOROW 9

// bedzie można użyć pipe() przy tworzeniu kolejnych rejestrów

void SIGUSR1_handle(int sig);
void SIGUSR2_handle(int sig);

int recieve_dyrektor(int sems, key_t *shared_mem, int result[]);

int main(int argc, char **argv)
{
    signal(SIGUSR1, SIGUSR1_handle);
    signal(SIGUSR2, SIGUSR2_handle);
    printf("rejestr\n");

    key_t key;
    key = atoi(argv[1]);

    int sems = semget(key, ILE_SEMAFOROW, 0); // semafory
    if (sems == -1)
    {
        perror("rejestr semget");
        exit(1);
    }

    int shm_id = shmget(key, sizeof(key_t), 0); // pamiec
    if (shm_id == -1)
    {
        perror("rejestr shmget");
        exit(1);
    }

    key_t *shared_mem = (key_t *)shmat(shm_id, NULL, 0); // podlaczamy pamiec
    if (shared_mem == (key_t *)-1)
    {
        perror("rejestr shmat");
        exit(1);
    }

    key_t tab[7]; // tab[0-4] - p_id, tab[5] - K, tab[6] - N
    if (recieve_dyrektor(sems, shared_mem, tab) != 0)
    {
        perror("rejestr recieve dyrektor");
        exit(1); // TODO: tu jest problem, nie ma mechanizmu do wykraczania jeśli podproces się wykraczy
    }

    printf("rejestr: otrzymano N=%d, K=%d, ", tab[6], tab[5]);
    for(int i=0; i<5; i++)
        printf("pid[%d]=%d, ", i, tab[i]);
    printf("\n");

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

int recieve_dyrektor(int sems, key_t *shared_mem, int result[])
{
    struct sembuf P = {.sem_num = SEMAFOR_REJESTR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < 7; i++)
    {
        while (semop(sems, &P, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("generator semop P");
                return 1;
            }
        }

        result[i] = (int)*shared_mem;

        while (semop(sems, &V, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("generator semop V");
                return 1;
            }
        }
    }
    return 0;
}