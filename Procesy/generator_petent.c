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

#define ILE_SEMAFOROW 6
#define SEMAFOR_DYREKTOR 1
#define SEMAFOR_GENERATOR 2
#define SEMAFOR_REJESTR 3
#define SEMAFOR_REJESTR_DWA 4
#define SEMAFOR_PETENCI 5

// volatile sig_atomic_t ODEBRAC = 0;

int tab[4]; // tab[0] - N, tab[1] - p_id[6], tab[2-3] - pidy kopii rejestrow

FILE *f;
time_t t;
struct tm *t_broken;

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void SIGUSR1_handle(int sig);
void SIGUSR2_handle(int sig);
void SIGRTMIN_handle(int sig);
void install_handler(int signo, void (*handler)(int));

int recieve_dyrektor(int sems, key_t *shared_mem, int result[]);
void recieve_rejestr();
void generate_petent(int N, key_t rejestr_pid[]);
int czy_limity_puste();
char *generate_name();
char *generate_surname();
char *generate_age();
// void // log_msg(char *msg);

int main(int argc, char **argv)
{
    raise(SIGSTOP);
    f = fopen("./Logi/petent", "w"); // otwieramy pusty plik dla petentow
    if (!f)
    {
        perror("generator fopen petent");
        exit(1);
    }

    f = fopen("./Logi/generator", "w");
    if (!f)
    {
        perror("generator fopen generator");
        exit(1);
    }

    // log_msg("generator uruchomiony");

    install_handler(SIGUSR1, SIGUSR1_handle);
    install_handler(SIGUSR2, SIGUSR2_handle);
    install_handler(SIGRTMIN, SIGRTMIN_handle);

    srand(time(NULL));

    printf("generator\n");

    key_t key;
    key = atoi(argv[1]);

    int sems = semget(key, ILE_SEMAFOROW, 0);
    if (sems == -1)
    {
        // log_msg("error semget main");
        perror("generator semget");
        exit(1);
    }

    int shm_id = shmget(key, sizeof(key_t), 0);
    if (shm_id == -1)
    {
        perror("generator shmget");
        // log_msg("error shmget main");
        exit(1);
    }

    key_t *shared_mem = (key_t *)shmat(shm_id, NULL, 0);
    if (shared_mem == (key_t *)-1)
    {
        perror("generator shmat");
        // log_msg("error shmat main");
        exit(1);
    }

    // log_msg("generator odbiera od dyrektora");
    for (int i = 0; i < 4; i++)
        tab[i] = -1;
    if (recieve_dyrektor(sems, shared_mem, tab) != 0)
    {
        perror("generator recieve dyrektor");
        // log_msg("error recieve_dyrektor");
        exit(1);
    }
    char message[110];
    sprintf(message, "generator odebral od dyrektora N=%d, p_id[6]=%d, tab[2]=%d, tab[3]=%d", tab[0], tab[1], tab[2], tab[3]);
    // log_msg(message);

    printf("generator - generowanie petentow\n");
    generate_petent(tab[0], &tab[1]);

    fclose(f);
    return 0;
}

void install_handler(int signo, void (*handler)(int))
{
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(signo, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }
}

void SIGUSR1_handle(int sig)
{
    exit(0);
}

void SIGUSR2_handle(int sig)
{
    exit(0);
}

void SIGRTMIN_handle(int sig)
{
    // niech wywołuje recieve_rejestr()
    // ODEBRAC = 1;
    recieve_rejestr();
}

int recieve_dyrektor(int sems, key_t *shared_mem, int result[])
{
    struct sembuf P = {.sem_num = SEMAFOR_GENERATOR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < 2; i++)
    {
        // log_msg("generator blokuje semafor GENERATOR");
        while (semop(sems, &P, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("generator semop P");
                // log_msg("error semop recieve_dyrektor");
                return 1;
            }
        }

        result[i] = (int)*shared_mem; // TODO: przesyłanie int a odbieranie key_t jest głupie

        // log_msg("generator oddaje semafor DYREKTOR");
        while (semop(sems, &V, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("generator semop V");
                // log_msg("error semop recieve_dyrektor");
                return 1;
            }
        }
    }
    return 0;
}

void generate_petent(int N, key_t rejestr_pid[])
{
    // log_msg("generator uruchamia generate_petent");
    struct sembuf P = {.sem_num = SEMAFOR_PETENCI, .sem_op = -1, .sem_flg = 0};

    key_t key = ftok(".", 1);
    if (key == -1)
    {
        perror("generator ftok");
        // log_msg("error ftok generate_petent");
        exit(1);
    }

    int sems = semget(key, ILE_SEMAFOROW, 0);
    if (sems == -1)
    {
        perror("generator semget");
        // log_msg("error semget generate_petent");
        exit(1);
    }

    char message[100];

    while (1)
    {
        if (czy_limity_puste())
        {
            printf("koniec limitow, nie mozna wpuscic wiecej petentow\n");
            // log_msg("limity urzednikow osiagniete, koniec pracy");
            return; // wracamy dla fclose()
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
        sprintf(message, "generator wysyla petenta do rejestru pid=%d", pid_to_use);
        // log_msg(message);

        // Konwertujemy PID na string
        char r_pid[32];
        sprintf(r_pid, "%d", pid_to_use);

        semop(sems, &P, 1); // czekamy aż możemy stworzyć petenta
        // log_msg("generator tworzy petenta");
        key_t pid = fork();
        if (pid == -1)
        {
            perror("generator fork");
            exit(1);
        }
        if (pid == 0)
        {
            srand(time(NULL) ^ getpid());
            // losujemy czy petent będzie VIP
            char vip[32];
            int v;
            v = rand() % 20 == 0 ? 1 : 0;
            sprintf(vip, "%d", v);
            if (kill(pid_to_use, 0) == -1) // sprawdzamy, czy rejestr dalej istnieje, potencjalnie dalej może zawieść
                if (errno == ESRCH)
                {
                    // log_msg("wybrany rejestr juz nie istnieje – losujemy ponownie");
                    sprintf(r_pid, "%d", rejestr_pid[0]); // awaryjnie wysyłamy do pierwszej kolejki
                }
            // bardzo źle, jeśli tu dostaniemy sygnał
            execl("Procesy/petent", "Procesy/petent", r_pid, generate_name(), generate_surname(), generate_age(), vip, NULL);
            perror("generator - execl petent");
            exit(1);
        }
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
    static char tab[sizeof(int) * 8];
    int k = rand() % 65 + 15;
    sprintf(tab, "%d", k);
    return tab;
}

void recieve_rejestr()
{
    // log_msg("generator uruchamia recieve_rejestr");
    key_t key = ftok(".", 1);
    if (key == -1)
    {
        perror("generator - ftok");
        // log_msg("error ftok main");
        exit(1);
    }

    int sems = semget(key, ILE_SEMAFOROW, 0);
    if (sems == -1)
    {
        perror("generator semget");
        // log_msg("error semget main");
        exit(1);
    }

    int shm_id = shmget(key, sizeof(key_t), 0);
    if (shm_id == -1)
    {
        perror("generator shmget");
        // log_msg("error shmget main");
        exit(1);
    }

    key_t *shared_mem = (key_t *)shmat(shm_id, NULL, 0);
    if (shared_mem == (key_t *)-1)
    {
        perror("generator shmat");
        // log_msg("error shmat main");
        exit(1);
    }

    struct sembuf P = {.sem_num = SEMAFOR_GENERATOR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_REJESTR, .sem_op = +1, .sem_flg = 0};

    // log_msg("generator odbiera dane");
    for (int i = 1; i < 4; i++)
    {
        // log_msg("generator blokuje semafor GENERATOR");
        while (semop(sems, &P, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("generator semop P");
                // log_msg("error semop rejestr");
                exit(1);
            }
        }

        tab[i] = *shared_mem;

        // log_msg("generator oddaje semafor REJESTR");
        while (semop(sems, &V, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("generator semop V");
                // log_msg("error semop rejestr");
                exit(1);
            }
        }
    }

    // log_msg("generator odebral pidy od rejestru");

    while (semop(sems, &V, 1) == -1) // dajemy znać rejestrowi, że generator na pewno skończył
    {
        if (errno == EINTR)
            continue;
        else
        {
            perror("generator semop V");
            // log_msg("error semop rejestr");
            exit(1);
        }
    }

    shmdt(shared_mem);
}

int czy_limity_puste()
{
    // log_msg("generator sprawdza limity urzednikow");
    struct sembuf P = {.sem_num = SEMAFOR_REJESTR_DWA, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_REJESTR_DWA, .sem_op = +1, .sem_flg = 0};

    key_t key = ftok(".", 1);
    if (key == -1)
    {
        perror("generator ftok");
        // log_msg("error ftok czy_limity");
        exit(1);
    }
    key_t key_tabx = ftok(".", 2);
    if (key_tabx == -1)
    {
        perror("generator ftok");
        // log_msg("error ftok tabx czy_limity");
        exit(1);
    }

    int sems = semget(key, ILE_SEMAFOROW, 0);
    if (sems == -1)
    {
        perror("generator semget");
        // log_msg("error semget cz_limity");
        exit(1);
    }

    // log_msg("generator blokuje semafor REJESTR_DWA");
    semop(sems, &P, 1);

    int shm_id = shmget(key_tabx, sizeof(int) * 5, 0);
    if (shm_id == -1)
    {
        perror("generator shmget tabx");
        // log_msg("generator oddaje semafor REJESTR_DWA");
        semop(sems, &V, 1);
        return 1;
    }

    key_t *tabx = (key_t *)shmat(shm_id, NULL, 0);
    if (tabx == (key_t *)-1)
    {
        perror("generator shmat");
        exit(1);
    }

    int flaga = 1;

    for (int i = 0; i < 5; i++)
        if (tabx[i] > 0)
        {
            flaga = 0;
            break;
        }
    // log_msg("generator oddaje semafor REJESTR_DWA");
    semop(sems, &V, 1);
    shmdt(tabx);

    return flaga;
}

// void // log_msg(char *msg)
// {
//     t = time(NULL);
//     t_broken = localtime(&t);
//     fprintf(f, "<%02d:%02d:%02d> %s\n", t_broken->tm_hour, t_broken->tm_min, t_broken->tm_sec, msg);
//     fflush(f);
// }