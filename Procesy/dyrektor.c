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

// TODO: deklaracja zmiennych, przesyłanie ich do odpowiednich procesów
// TODO: przesłać N i K do rejestru
// TODO: przesłać pidy urzednikow do rejestru

#define ILE_SEMAFOROW 9
#define SEMAFOR_MAIN 0
#define SEMAFOR_DYREKTOR 1
#define SEMAFOR_GENERATOR 2
#define SEMAFOR_REJESTR 3
#define ILE_PROCESOW 8

time_t Tp, Tk;
int N, K; // do generator, rejestr

key_t key;
key_t p_id[ILE_PROCESOW];

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void cleanup();
void SIGINT_handle(int sig);
int recieve_main(int sems, key_t *shared_mem);
int send_generator(int sems, key_t *shared_mem);
int send_rejestr(int sems, key_t *shared_mem);

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

    if (recieve_main(sems, shared_mem) != 0)
    {
        perror("dyrektor recieve_main");
        cleanup();
        exit(1);
    }

    if (shmdt(shared_mem) != 0)
        perror("dyrektor shmdt");

    printf("dyrektor otrzymal:\n");
    for (int i = 0; i < ILE_PROCESOW; i++)
    {
        printf("%d\n", p_id[i]);
        // kill(p_id[i], SIGUSR1);
        // kill(p_id[i], SIGUSR2);
    }

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
    for (int i = 0; i < ILE_PROCESOW; i++)
        kill(p_id[i], SIGINT);
}

void SIGINT_handle(int sig)
{
    printf("\nprzechwycono SIGINT\n");
    cleanup();
    exit(0);
}

int recieve_main(int sems, key_t *shared_mem)
{
    struct sembuf P = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_MAIN, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < ILE_PROCESOW; i++) // odbieramy p_id[]
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

        p_id[i] = *shared_mem;

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

int send_generator(int sems, key_t *shared_mem)
{
    // idea - mamy jeden adres pamieci
    // dyrektor zmienia semafory procesow, ktore chce aby z niego akurat czytaly
    struct sembuf P = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_GENERATOR, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < 2; i++) // odbieramy p_id[]
    {
        while (semop(sems, &P, 1) == -1) // czekamy czy można wysyłać
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("main semop P");
                cleanup();
                exit(1);
            }
        }

        *shared_mem = i == 0 ? p_id[5] : N;

        while (semop(sems, &V, 1) == -1) // zaznaczamy, że można czytać
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("main semop V");
                cleanup();
                exit(1);
            }
        }
    }
    return 0;
}

int send_rejestr(int sems, key_t *shared_mem)
{
    struct sembuf P = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_REJESTR, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < 7; i++) // odbieramy p_id[]
    {
        while (semop(sems, &P, 1) == -1) // czekamy czy można wysyłać
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("main semop P");
                cleanup();
                exit(1);
            }
        }

        switch (i)
        {
        case 5:
            *shared_mem = K;
            break;
        case 6:
            *shared_mem = N;
            break;

        default:
            *shared_mem = p_id[i];
            break;
        }

        while (semop(sems, &V, 1) == -1) // zaznaczamy, że można czytać
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("main semop V");
                cleanup();
                exit(1);
            }
        }
    }
    return 0;
}