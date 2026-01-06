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

// usuwa semafory, pamiec wspoldzielona i zamyka wszystkie procesy
void cleanup();

int maint(int argc, char **argv)
{
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
    int sems = semget(key, 2, IPC_CREAT | 0666);
    if (sems == -1)
    {
        perror("main - semget");
        // cleanup();
        exit(1);
    }
    // uruchamiamy proces dyrektor
    pid_t p_id = fork();
    if (p_id == 0)
    {
        execl("/Procesy/dyrektor", "/Procesy/dyrektor", key_str, NULL);
        perror("main - execl dyrektor");
        // cleanup();
        exit(1);
    }
    // uruchamiamy procesy Rejestracja
    p_id = fork();
    if (p_id == 0)
    {
        execl("/Procesy/rejestr", "/Procesy/rejestr", key_str, NULL);
        perror("main - execl rejestr");
        // cleanup();
        exit(1);
    }
    // uruchamiamy procesy urzednik
    for (int i = 0; i < 6; i++)
    {
        p_id = fork();
        if (p_id == 0)
        {
            char u_id[2];
            i == 5 ? sprintf(u_id, "%d", i - 1) : sprintf(u_id, "%d", i);
            execl("/Procesy/rejestr", "/Procesy/rejestr", key_str, u_id, NULL);
            perror("main - execl rejestr");
            // cleanup();
            exit(1);
        }
    }
    // uruchamiamy procesy petent
    p_id = fork();
    if (p_id == 0)
    {
        execl("/Procesy/generator", "/Procesy/generator", key_str, NULL);
        perror("main - execl generator");
        // cleanup();
        exit(1);
    }
    return 0;
}