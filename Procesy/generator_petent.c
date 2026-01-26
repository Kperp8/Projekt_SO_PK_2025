#define _POSIX_C_SOURCE 200809L

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
#include <time.h>
#include <string.h>

#define ILE_SEMAFOROW 9
#define SEMAFOR_DYREKTOR 1
#define SEMAFOR_GENERATOR 2
#define SEMAFOR_REJESTR 3
#define SEMAFOR_REJESTR_DWA 4

volatile sig_atomic_t ODEBRAC = 0;

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void SIGUSR1_handle(int sig);
void SIGUSR2_handle(int sig);
void SIGRTMIN_handle(int sig);

int recieve_dyrektor(int sems, key_t *shared_mem, int result[]);
void recieve_rejestr(key_t tab[]);
void generate_petent(int N, key_t rejestr_pid[]);
int czy_limity_puste();
char *generate_name();
char *generate_surname();
char *generate_age();

int main(int argc, char **argv)
{
    struct sigaction sa;

    /* SIGUSR1 */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIGUSR1_handle;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    /* SIGUSR2 */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIGUSR2_handle;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR2, &sa, NULL);

    /* SIGRTMIN */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIGRTMIN_handle;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGRTMIN, &sa, NULL);

    srand(time(NULL));

    printf("generator\n");

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

    key_t tab[4] = {-1, -1, -1, -1}; // tab[0] - N, tab[1] - p_id[6], tab[2-3] - pidy kopii rejestrow
    if (recieve_dyrektor(sems, shared_mem, tab) != 0)
    {
        perror("generator recieve dyrektor");
        exit(1); // TODO: tu jest problem, nie ma mechanizmu do wykraczania jeśli podproces się wykraczy
    }

    // printf("generator: otrzymano N=%d, pid=%d\n", tab[0], tab[1]);

    printf("generator - generowanie petentow\n");
    generate_petent(tab[0], &tab[1]); // TODO: jeśli p_id[2-3] nie == -1, wysyłaj losowo pomiędzy nie

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

void SIGRTMIN_handle(int sig)
{
    ODEBRAC = 1;
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

void generate_petent(int N, key_t rejestr_pid[])
{
    int active_petents = 0;

    while (1) // TODO: docelowo while(1) z kontrolą liczby petentów
    {
        // sleep(1);

        if (ODEBRAC)
        {
            recieve_rejestr(rejestr_pid);
            ODEBRAC = 0;
        }

        if (czy_limity_puste())
        {
            printf("koniec limitow, nie mozna wpuscic wiecej petentow\n");
            exit(0);
        }

        // Tworzymy tablicę aktywnych rejestrów, zawsze z głównym [0]
        key_t pool[3];
        int pool_size = 1;
        pool[0] = rejestr_pid[0];

        if (rejestr_pid[1] != -1)
            pool[pool_size++] = rejestr_pid[1];
        if (rejestr_pid[2] != -1)
            pool[pool_size++] = rejestr_pid[2];

        // Losowo wybieramy jeden PID z puli (główny zawsze jest obecny)
        key_t pid_to_use = pool[rand() % pool_size];

        // Konwertujemy PID na string
        char r_pid[32];
        snprintf(r_pid, sizeof(r_pid), "%d", pid_to_use);

        // Tworzymy proces petenta, jeśli jest miejsce
        if (active_petents < N)
        {
            key_t pid = fork();
            if (pid == -1)
            {
                perror("generator fork");
                exit(1);
            }
            if (pid == 0)
            {
                srand(time(NULL) ^ getpid());
                execl("Procesy/petent", "Procesy/petent",
                      r_pid, generate_name(), generate_surname(), generate_age(), NULL);
                perror("generator - execl petent");
                exit(1);
            }
            active_petents++;
        }

        // Sprawdzamy zakończone procesy
        int status;
        pid_t wpid;
        while ((wpid = waitpid(-1, &status, WNOHANG)) > 0)
            active_petents--;

        // printf("aktywnych petentow: %d\n", active_petents);
    }
}

char *generate_name()
{
    static char *imiona[] = {
        "Jan",
        "Marcin",
        "Anna",
        "Katarzyna",
        "Piotr",
        "Marek",
        "Ewa",
        "Tomasz",
        "Monika"};
    return imiona[rand() % 9];
}

char *generate_surname()
{
    static char *nazwiska[] = {
        "Kowalski",
        "Duda",
        "Nowak",
        "Wiśniewski",
        "Wójcik",
        "Kowalczyk",
        "Kamiński",
        "Lewandowski",
        "Szymański"};
    return nazwiska[rand() % 9];
}

char *generate_age()
{
    static char tab[sizeof(int) * 8]; // static pozwala istnieć po zakończeniu funkcji
    int k = rand() % 65 + 15;
    sprintf(tab, "%d", k);
    return tab;
}

void recieve_rejestr(key_t pid[]) // TODO: na razie troche brzydko, przemyśleć
{
    key_t key = ftok(".", 1);
    if (key == -1)
    {
        perror("generator - ftok");
        exit(1);
    }

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

    struct sembuf P = {.sem_num = SEMAFOR_GENERATOR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_REJESTR, .sem_op = +1, .sem_flg = 0};

    for (int i = 1; i < 4; i++)
    {
        while (semop(sems, &P, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("generator semop P");
                exit(1);
            }
        }

        pid[i] = *shared_mem;

        while (semop(sems, &V, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("generator semop V");
                exit(1);
            }
        }
    }
    shmdt(shared_mem);
    printf("generator odebral pidy rejestrow\n");
}

int czy_limity_puste()
{
    struct sembuf P = {.sem_num = SEMAFOR_REJESTR_DWA, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_REJESTR_DWA, .sem_op = +1, .sem_flg = 0};

    key_t key = ftok(".", 1);
    if (key == -1)
    {
        perror("generator ftok");
        exit(1);
    }
    key_t key_tabx = ftok(".", 2);
    if (key == -1)
    {
        perror("generator ftok");
        exit(1);
    }

    int sems = semget(key, ILE_SEMAFOROW, 0); // semafory
    if (sems == -1)
    {
        perror("generator semget");
        exit(1);
    }

    semop(sems, &P, 1);

    int shm_id = shmget(key_tabx, sizeof(int) * 5, 0); // pamiec
    if (shm_id == -1)
    {
        perror("generator shmget tabx");
        semop(sems, &V, 1);
        return 0;
    }

    key_t *tabx = (key_t *)shmat(shm_id, NULL, 0); // podlaczamy pamiec
    if (tabx == (key_t *)-1)
    {
        perror("generator shmat");
        exit(1);
    }

    int flaga = 1;

    for (int i = 0; i < 5; i++)
        if (tabx[i] != 0)
        {
            flaga = 0;
            break;
        }
    semop(sems, &V, 1);
    shmdt(tabx);

    return flaga;
}