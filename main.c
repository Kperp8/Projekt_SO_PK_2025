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

#define ILE_SEMAFOROW 9 // po jednym dla main, dyrektor, rejestr, każdego urzędnika
#define SEMAFOR_MAIN 0
#define SEMAFOR_DYREKTOR 1
#define ILE_PROCESOW 8

/*
0-4: urzednicy
5: rejestr
6: generator
7: dyrektor
*/
key_t p_id[ILE_PROCESOW]; // tablica pid procesow potomnych
key_t key;

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// usuwa semafory, pamiec wspoldzielona i zamyka wszystkie procesy
void cleanup();

void SIGINT_handle(int sig);

int main(int argc, char **argv)
{
    signal(SIGINT, SIGINT_handle);

    int n = -1;
    // tworzymy klucz
    key = ftok(".", 1);
    // printf("key - %d\n", key);
    if (key == -1)
    {
        perror("main - ftok");
        exit(1);
    }
    char key_str[sizeof(key_t) * 8];
    sprintf(key_str, "%d", key);

    // tworzymy semafory
    int sems = semget(key, ILE_SEMAFOROW, IPC_CREAT | 0666);
    if (sems == -1)
    {
        perror("main - semget");
        cleanup();
        exit(1);
    }

    // tworzymy pamiec dzieloną dla dyrektora
    int shmid = shmget(key, sizeof(key_t), IPC_CREAT | 0666);
    if (shmid == -1)
    {
        perror("main shmget");
        cleanup();
        exit(1);
    }

    // uruchamiamy procesy urzednik
    for (int i = 0; i < 6; i++)
    {
        p_id[++n] = fork();
        if (p_id[n] == 0)
        {
            char u_id[2]; // dodatkowy identyfikator rodzaju urzednika
            i == 5 ? sprintf(u_id, "%d", i - 1) : sprintf(u_id, "%d", i);
            execl("Procesy/urzednik", "Procesy/urzednik", key_str, u_id, NULL);
            perror("main - execl urzednik");
            cleanup();
            exit(1);
        }
    }

    // uruchamiamy procesy Rejestracja
    p_id[++n] = fork();
    if (p_id[n] == 0)
    {
        execl("Procesy/rejestr", "Procesy/rejestr", key_str, NULL);
        perror("main - execl rejestr");
        cleanup();
        exit(1);
    }

    // uruchamiamy proces generator
    p_id[++n] = fork();
    if (p_id[n] == 0)
    {
        execl("Procesy/generator_petent", "Procesy/generator_petent", key_str, NULL);
        perror("main - execl generator");
        cleanup();
        exit(1);
    }

    // uruchamiamy proces dyrektor
    p_id[++n] = fork();
    if (p_id[n] == 0)
    {
        execl("Procesy/dyrektor", "Procesy/dyrektor", key_str, NULL); // TODO: prześlij p_id[]
        perror("main - execl dyrektor");
        cleanup();
        exit(1);
    }

    // wysyłamy p_id do dyrektora
    key_t *shared_mem = (key_t *)shmat(shmid, NULL, 0);
    if (shared_mem == (key_t *)-1)
    {
        perror("main shmat");
        cleanup();
        exit(1);
    }

    // wysyłamy
    // printf("main - wysyla\n");
    union semun arg;
    arg.val = 1;
    if (semctl(sems, SEMAFOR_MAIN, SETVAL, arg) == -1)
    {
        perror("main semctl");
        cleanup();
        exit(1);
    }
    arg.val = 0;
    if (semctl(sems, SEMAFOR_DYREKTOR, SETVAL, arg) == -1)
    {
        perror("main semctl");
        cleanup();
        exit(1);
    }
    struct sembuf P = {.sem_num = SEMAFOR_MAIN, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = +1, .sem_flg = 0};
    for (int i = 0; i < ILE_PROCESOW; i++)
    {
        while (semop(sems, &P, 1) == -1)
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

        *shared_mem = p_id[i];

        while (semop(sems, &V, 1) == -1)
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

    shmdt(shared_mem);

    // for (int i = 0; i < ILE_PROCESOW; i++)
    // waitpid(p_id[i], NULL, 0);

    // cleanup();

    return 0;
}

void cleanup()
{
    int semid = semget(key, 0, 0);
    if (semid != -1)
        semctl(semid, 0, IPC_RMID);
    // usuwamy dzieloną pamięć
    int shmid = shmget(key, sizeof(key_t), 0);
    if (shmid != -1)
        shmctl(shmid, IPC_RMID, NULL);
    // zamykamy procesy pochodne SIGINTem
    for (int i = 0; i < ILE_PROCESOW; i++)
        kill(p_id[i], SIGINT);
}

void SIGINT_handle(int sig)
{
    printf("\nprzechwycono SIGINT\n");
    cleanup();
    exit(0);
}