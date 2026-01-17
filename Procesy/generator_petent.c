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

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void SIGUSR1_handle(int sig);
void SIGUSR2_handle(int sig);

int recieve_dyrektor(int sems, key_t *shared_mem, int result[]);
void generate_petent(int N, key_t rejestr_pid);
char *generate_name(); // problem na przyszłość: pamięć dynamiczna, może być źle
char *generate_surname();
char *generate_age();

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

    key_t tab[2]; // tab[0] - N, tab[1] - p_id[5]
    if (recieve_dyrektor(sems, shared_mem, tab) != 0)
    {
        perror("generator recieve dyrektor");
        exit(1); // TODO: tu jest problem, nie ma mechanizmu do wykraczania jeśli podproces się wykraczy
    }

    // printf("generator: otrzymano N=%d, pid=%d\n", tab[0], tab[1]);

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
    struct sembuf P = {.sem_num = SEMAFOR_GENERATOR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < 2; i++)
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

        result[i] = (int)*shared_mem; // TODO: przesyłanie int a odbieranie key_t jest głupie

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

void generate_petent(int N, key_t rejestr_pid)
{
    int active_petents = 0; // ile petentów jest w danej chwili
    char r_pid[sizeof(key_t) * 8];
    sprintf(r_pid, "%d", r_pid);
    while (1) // TODO: niebiezpieczne, przemyśleć
    {
        // TODO: wygeneruj petentowi imie i mu je przekaż
        // TODO: wygeneruj petentowi wiek, i jeśli <18 podaj mu rodzica czy coś
        // jeśli liczba petentów < N, generuj petenta
        if (active_petents < N)
        {
            key_t pid = fork();
            if (pid == 0)
            {
                execl("./Procesy/petent", "./Procesy/petent", r_pid, generate_name(), generate_surname(), generate_age(), NULL);
                perror("rejestr - execl rejestr"); // TODO: nie ma mechanizmu jeśli proces potomny się zepsuje
            }
            active_petents++;
        }

        // sprawdzamy ile procesów się zakończyło
        int status;
        pid_t wpid;
        while ((wpid = waitpid(-1, &status, WNOHANG)) > 0)
            active_petents--;
    }
}