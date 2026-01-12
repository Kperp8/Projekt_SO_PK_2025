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

int ILE_SEMAFOROW = 8;
int SEMAFOR_MAIN = 0;
int SEMAFOR_DYREKTOR = 1;
int ILE_POCHODNYCH = 8;

void cleanup(key_t key, key_t p_id[]);

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int main(int argc, char **argv)
{
    printf("dyrektor online\n");
    key_t key = atoi(argv[1]); // odbieramy klucz
    key_t p_id[ILE_POCHODNYCH];

    int sems = semget(key, ILE_SEMAFOROW, 0); // semafory
    if (sems == -1)
    {
        perror("dyrektor semget");
        exit(1);
    }

    int shm_id = shmget(key, sizeof(key_t) * ILE_POCHODNYCH, 0); // pamiec
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

    for (int i = 0; i < ILE_POCHODNYCH; i++) // odbieramy p_id[]
    {
        struct sembuf P = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = -1, .sem_flg = 0};
        struct sembuf V = {.sem_num = SEMAFOR_MAIN, .sem_op = +1, .sem_flg = 0};

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

    cleanup(key, p_id);

    return 0;
}

void cleanup(key_t key, key_t p_id[])
{
    int semid = semget(key, 0, 0);
    if (semid != -1)
        semctl(semid, 0, IPC_RMID);
    for (int i = 0; i < ILE_POCHODNYCH; i++)
        kill(p_id[i], SIGINT);
    int shmid = shmget(key, sizeof(key_t), 0);
    if (shmid != -1)
        shmctl(shmid, IPC_RMID, NULL);
}