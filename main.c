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

int ILE_SEMAFOROW = 8; // po jednym dla main, dyrektor, każdego urzędnika
int SEMAFOR_MAIN = 0;
int SEMAFOR_DYREKTOR = 1;
int ILE_POCHODNYCH = 8;

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// usuwa semafory, pamiec wspoldzielona i zamyka wszystkie procesy
void cleanup(key_t key, key_t p_id[]);

int main(int argc, char **argv)
{
    key_t p_id[ILE_POCHODNYCH]; // tablica pid procesow potomnych
    int n = -1;
    // tworzymy klucz
    key_t key = ftok(".", 1);
    if (key == -1)
    {
        perror("main - ftok");
        exit(1);
    }
    char key_str[20];
    sprintf(key_str, "%d", key);

    // tworzymy semafory
    int sems = semget(key, ILE_SEMAFOROW, IPC_CREAT | 0666);
    if (sems == -1)
    {
        perror("main - semget");
        cleanup(key, p_id);
        exit(1);
    }

    // tworzymy pamiec dzieloną dla dyrektora
    int shmid = shmget(key, sizeof(key_t), IPC_CREAT | 0666);
    if (shmid == -1)
    {
        perror("semget");
        cleanup(key, p_id);
        exit(1);
    }

    // uruchamiamy procesy Rejestracja
    p_id[++n] = fork();
    if (p_id[n] == 0)
    {
        execl("./Procesy/rejestr", "./Procesy/rejestr", key_str, NULL);
        perror("main - execl rejestr");
        cleanup(key, p_id);
        exit(1);
    }

    // uruchamiamy procesy urzednik
    for (int i = 0; i < 6; i++)
    {
        p_id[++n] = fork();
        if (p_id[n] == 0)
        {
            char u_id[2];
            i == 5 ? sprintf(u_id, "%d", i - 1) : sprintf(u_id, "%d", i);
            execl("./Procesy/urzednik", "./Procesy/urzednik", key_str, u_id, NULL);
            perror("main - execl urzednik");
            cleanup(key, p_id);
            exit(1);
        }
    }

    // uruchamiamy procesy petent
    p_id[++n] = fork();
    if (p_id[n] == 0)
    {
        execl("./Procesy/generator", "./Procesy/generator", key_str, NULL);
        perror("main - execl generator");
        cleanup(key, p_id);
        exit(1);
    }

    // uruchamiamy proces dyrektor
    p_id[++n] = fork();
    if (p_id[n] == 0)
    {
        execl("./Procesy/dyrektor", "./Procesy/dyrektor", key_str, NULL); // TODO: prześlij p_id[]
        perror("main - execl dyrektor");
        cleanup(key, p_id);
        exit(1);
    }

    // wysyłamy p_id do dyrektora
    key_t *shared_mem = (key_t *)shmat(shmid, NULL, 0);
    if (shared_mem == (key_t *)-1)
    {
        perror("main shmat");
        cleanup(key, p_id);
        exit(1);
    }

    // wysyłamy
    struct sembuf P = {.sem_num = SEMAFOR_MAIN, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = +1, .sem_flg = 0};
    for (int i = 0; i < ILE_POCHODNYCH; i++)
    {
        while (semop(sems, &P, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("main semop P");
                cleanup(key, p_id);
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
                perror("semop V");
                cleanup(key, p_id);
                exit(1);
            }
        }
    }

    shmdt(shared_mem);

    for (int i = 0; i < ILE_POCHODNYCH; i++)
        waitpid(p_id[i], NULL, 0);

    cleanup(key, p_id);

    return 0;
}

void cleanup(key_t key, key_t p_id[])
{
    int semid = semget(key, 0, 0);
    if (semid != -1)
        semctl(semid, 0, IPC_RMID);
    // zamykamy procesy pochodne SIGINTem
    for (int i = 0; i < ILE_POCHODNYCH; i++)
        kill(p_id[i], SIGINT);
    // usuwamy dzieloną pamięć
    int shmid = shmget(key, sizeof(key_t), 0);
    if (shmid != -1)
        shmctl(shmid, IPC_RMID, NULL);
}