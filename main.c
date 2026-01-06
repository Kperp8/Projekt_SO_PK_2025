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

int ILE_SEMAFOROW = 5;
int ILE_POCHODNYCH = 8;

// usuwa semafory, pamiec wspoldzielona i zamyka wszystkie procesy
void cleanup(key_t key, key_t p_id[]);

int maint(int argc, char **argv)
{
    key_t p_id[ILE_POCHODNYCH]; // tablica pid procesow potomnych
    int n = -1;
    // tworzymy klucz
    key_t key = ftok("seed", 1);
    if (key == -1)
    {
        perror("main - ftok");
        exit(1);
    }
    char key_str[5];
    sprintf(key_str, "%d", key);

    // tworzymy semafory
    int sems = semget(key, ILE_SEMAFOROW, IPC_CREAT | 0666);
    if (sems == -1)
    {
        perror("main - semget");
        cleanup(key, p_id);
        exit(1);
    }

    // uruchamiamy proces dyrektor
    p_id[++n] = fork();
    if (p_id == 0)
    {
        execl("/Procesy/dyrektor", "/Procesy/dyrektor", key_str, NULL);
        perror("main - execl dyrektor");
        cleanup(key, p_id);
        exit(1);
    }

    // uruchamiamy procesy Rejestracja
    p_id[++n] = fork();
    if (p_id == 0)
    {
        execl("/Procesy/rejestr", "/Procesy/rejestr", key_str, NULL);
        perror("main - execl rejestr");
        cleanup(key, p_id);
        exit(1);
    }

    // uruchamiamy procesy urzednik
    for (int i = 0; i < 6; i++)
    {
        p_id[++n] = fork();
        if (p_id == 0)
        {
            char u_id[2];
            i == 5 ? sprintf(u_id, "%d", i - 1) : sprintf(u_id, "%d", i);
            execl("/Procesy/rejestr", "/Procesy/rejestr", key_str, u_id, NULL);
            perror("main - execl rejestr");
            cleanup(key, p_id);
            exit(1);
        }
    }

    // uruchamiamy procesy petent
    p_id[++n] = fork();
    if (p_id == 0)
    {
        execl("/Procesy/generator", "/Procesy/generator", key_str, NULL);
        perror("main - execl generator");
        cleanup(key, p_id);
        exit(1);
    }
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
}