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

// TODO: wysyłanie sygnałów do procesów

#define ILE_SEMAFOROW 8
#define SEMAFOR_MAIN 0
#define SEMAFOR_DYREKTOR 1
#define ILE_POCHODNYCH 8

key_t key;
key_t p_id[ILE_POCHODNYCH];

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void cleanup();
void SIGINT_handle(int sig);

int main(int argc, char **argv)
{
    signal(SIGINT, SIGINT_handle);

    printf("dyrektor\n");
    key = atoi(argv[1]); // odbieramy klucz

    int sems = semget(key, ILE_SEMAFOROW, 0); // semafory
    if (sems == -1)
    {
        perror("dyrektor semget");
        exit(1);
    }

    int shm_id = shmget(key, sizeof(key_t), 0); // pamiec
    if (shm_id == -1)
    {
        perror("dyrektor shmget");
        exit(1);
    }

    key_t *shared_mem = (key_t *)shmat(shm_id, NULL, 0); // podlaczamy pamiec
    if (shared_mem == (key_t *)-1)
    {
        perror("dyrektor shmat");
        exit(1);
    }

    struct sembuf P = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_MAIN, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < ILE_POCHODNYCH; i++) // odbieramy p_id[]
    {
        while (semop(sems, &P, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("dyrektor semop P");
                exit(1);
            }
        }

        p_id[i] = *shared_mem;

        while (semop(sems, &V, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("dyrektor semop V");
                exit(1);
            }
        }
    }

    if (shmdt(shared_mem) != 0)
        perror("dyrektor shmdt");

    printf("dyrektor otrzymal i wysyla sygnaly 1 i 2:\n");
    for (int i = 0; i < ILE_POCHODNYCH; i++)
    {
        printf("%d\n", p_id[i]);
        kill(p_id[i], SIGUSR1);
        kill(p_id[i], SIGUSR2);
    }

    sleep(10);

    cleanup();

    return 0;
}

void cleanup()
{
    int semid = semget(key, 0, 0);
    if (semid != -1)
        semctl(semid, 0, IPC_RMID);
    int shmid = shmget(key, sizeof(key_t), 0);
    if (shmid != -1)
    {
        key_t *shared_mem = (key_t *)shmat(shmid, NULL, 0);
        if (shmdt(shared_mem) != 0)
            perror("dyrektor shmdt");
        shmctl(shmid, IPC_RMID, NULL);
    }
    for (int i = 0; i < ILE_POCHODNYCH; i++)
        kill(p_id[i], SIGINT);
}

void SIGINT_handle(int sig)
{
    printf("\nprzechwycono SIGINT\n");
    cleanup();
    exit(0);
}