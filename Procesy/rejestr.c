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
#include <sys/msg.h>
#include <time.h>

#define SEMAFOR_DYREKTOR 1
#define SEMAFOR_GENERATOR 2
#define SEMAFOR_REJESTR 3
#define SEMAFOR_REJESTR_DWA 4 // semafor dla komunikacji między rejestrami
#define ILE_SEMAFOROW 9

volatile sig_atomic_t CLOSE = 0;

FILE *f;
time_t t;
struct tm *t_broken;

int tab_X[5] = {
    150,  // X1
    600,  // X2
    300,  // X3
    400,  // X4
    1000, // X5
};

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

struct msgbuf_rejestr // wiadomość od rejestru
{
    long mtype;
    pid_t pid;
} __attribute__((packed));

void SIGUSR1_handle(int sig);
void SIGUSR2_handle(int sig);
void SIGINT_handle(int sig);
void install_handler(int signo, void (*handler)(int));

int recieve_dyrektor(int sems, key_t *shared_mem, int result[]);
void handle_petent(int pid[]);
int choose_pid(int sems, int tab[]);
void check_petenci(int N, int K, key_t key, long *shared_mem, pid_t pid[], pid_t pid_generator, int tab[]); // sprawdza ile jest petentow w kolejce, otwiera nowe procesy rejestr
void send_generator(pid_t pid[], pid_t pid_generator);
void cleanup();
void cleanup_klon();
void handle_petent_klon(int pid[]);
void log_msg(char *msg);

int main(int argc, char **argv)
{
    install_handler(SIGUSR1, SIGUSR1_handle);
    install_handler(SIGUSR2, SIGUSR2_handle);
    install_handler(SIGINT, SIGINT_handle);
    raise(SIGSTOP);
    srand(time(NULL));
    printf("rejestr %d\n", getpid());

    f = fopen("./Logi/rejestr_1", "w"); // otwieramy dla klonów
    if (!f)
    {
        perror("rejestr fopen");
        cleanup();
        exit(1);
    }
    fclose(f);
    f = fopen("./Logi/rejestr_2", "w");
    if (!f)
    {
        perror("rejestr fopen");
        cleanup();
        exit(1);
    }
    fclose(f);
    f = fopen("./Logi/rejestr_0", "w");
    if (!f)
    {
        perror("rejestr fopen");
        cleanup();
        exit(1);
    }

    log_msg("rejestr sie uruchamia");
    key_t key;
    key = atoi(argv[1]);

    int sems = semget(key, ILE_SEMAFOROW, 0);
    if (sems == -1)
    {
        perror("rejestr semget");
        log_msg("error semget main");
        exit(1);
    }

    int shm_id = shmget(key, sizeof(key_t), 0);
    if (shm_id == -1)
    {
        perror("rejestr shmget 1");
        log_msg("error shmget main");
        exit(1);
    }

    key_t *shared_mem = (key_t *)shmat(shm_id, NULL, 0);
    if (shared_mem == (key_t *)-1)
    {
        perror("rejestr shmat");
        log_msg("error shmat main");
        exit(1);
    }

    log_msg("rejestr odbiera od dyrektor");
    key_t tab[8]; // tab[0-4] - p_id, tab[5] - K, tab[6] - N, tab[7] - p_id[7]
    if (recieve_dyrektor(sems, shared_mem, tab) != 0)
    {
        perror("rejestr recieve dyrektor");
        log_msg("error recieve_dyrektor");
        exit(1);
    }
    shmdt(shared_mem);

    char message[120];
    sprintf(message, "rejestr odebral od dyrektora N=%d, K=%d, pid[7]=%d", tab[6], tab[5], tab[7]);
    log_msg(message);
    printf("rejestr: otrzymano N=%d, K=%d, pid[7]=%d, ", tab[6], tab[5], tab[7]);
    for (int i = 0; i < 5; i++)
        printf("pid[%d]=%d, ", i, tab[i]);
    sprintf(message, "pidy urzedniko pid[0]=%d, pid[1]=%d, pid[2]=%d, pid[3]=%d, pid[4]=%d", tab[0], tab[1], tab[2], tab[3], tab[4]);
    log_msg(message);
    printf("\n");

    log_msg("rejestr ustawia wartosci semaforow");
    union semun arg;
    arg.val = 1;
    if (semctl(sems, SEMAFOR_REJESTR, SETVAL, arg) == -1)
    {
        perror("rejestr semctl");
        log_msg("error semctl SETVAL");
        cleanup();
        exit(1);
    }
    arg.val = 0;
    if (semctl(sems, SEMAFOR_GENERATOR, SETVAL, arg) == -1)
    {
        perror("rejestr semctl");
        log_msg("error semctl SETVAL");
        cleanup();
        exit(1);
    }

    key_t key_tabx = ftok(".", 2);
    if (key_tabx == -1)
    {
        perror("rejestr ftok tabx main");
        log_msg("error ftok tabx main");
        cleanup();
        exit(1);
    }

    // zapisujemy tab_X do pamięci współdzielonej, ponieważ procesy potomne
    log_msg("rejestr zapisuje tabx do pamieci wspoldzielonej");
    shm_id = shmget(key_tabx, sizeof(int) * 5, IPC_CREAT | 0666);
    if (shm_id == -1)
    {
        perror("rejestr shmget 2");
        log_msg("error shmget main");
        exit(1);
    }

    shared_mem = (key_t *)shmat(shm_id, NULL, 0);
    if (shared_mem == (key_t *)-1)
    {
        perror("rejestr shmat");
        log_msg("error shmat main");
        cleanup();
        exit(1);
    }
    for (int i = 0; i < 5; i++)
        shared_mem[i] = tab_X[i];

    shmdt(shared_mem);

    arg.val = 1;
    if (semctl(sems, SEMAFOR_REJESTR_DWA, SETVAL, arg) == -1)
    {
        perror("rejestr semctl");
        log_msg("error semop SETVAL");
        cleanup();
        exit(1);
    }

    handle_petent(tab);

    printf("rejestr sie zamyka\n");
    cleanup();
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
    CLOSE = 1;
}

void SIGUSR2_handle(int sig)
{
    log_msg("rejestr przechwycil SIGUSR2");
    cleanup();
    exit(0);
}

void SIGINT_handle(int sig)
{
    CLOSE = 1;
}

int recieve_dyrektor(int sems, key_t *shared_mem, int result[])
{
    log_msg("rejestr uruchamia recieve_dyrektor");
    struct sembuf P = {.sem_num = SEMAFOR_REJESTR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < 8; i++)
    {
        log_msg("rejestr blokuje semafor REJESTR");
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

        result[i] = (int)*shared_mem;

        log_msg("rejestr oddaje semafor DYREKTOR");
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

int choose_pid(int sems, int tab[])
{
    log_msg("rejestr losuje pid");
    struct sembuf P = {.sem_num = SEMAFOR_REJESTR_DWA, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_REJESTR_DWA, .sem_op = +1, .sem_flg = 0};
    log_msg("rejestr blokuje semafor REJESTR_DWA");
    semop(sems, &P, 1);
    int i, n = 0;
    do
    {
        i = rand() % 10;
        i = i < 4 ? i : 4;
        if (tab[i] == 0)
            n++;
        if (n == 50) // TODO: EKSTREMALNIE głupie
            break;
    } while (tab[i] == 0);
    tab[i]--;
    log_msg("rejestr oddaje semafor REJESTR_DWA");
    semop(sems, &V, 1);
    return i;
}

void handle_petent(int pid[])
{
    log_msg("rejestr uruchamia handle_petent");
    // tworzymy klucz z maską pid rejestru
    key_t key = ftok(".", getpid());
    if (key == -1)
    {
        perror("rejestr ftok");
        log_msg("error ftok handle_petent");
        exit(1);
    }

    key_t key_tabx = ftok(".", 2);
    if (key_tabx == -1)
    {
        perror("rejestr ftok");
        log_msg("error ftok tabx handle_petent");
        exit(1);
    }

    key_t key_main = ftok(".", 1);
    if (key_tabx == -1)
    {
        perror("rejestr ftok");
        log_msg("error ftok main handle_petent");
        exit(1);
    }

    // tworzymy semafor do liczby czekających
    int sems = semget(key, 1, IPC_CREAT | 0666);
    if (sems == -1)
    {
        perror("rejestr semget");
        log_msg("error semget handle_petent");
        cleanup();
        exit(1);
    }

    int sems_2 = semget(key_main, ILE_SEMAFOROW, 0);
    if (sems_2 == -1)
    {
        perror("rejestr semget");
        log_msg("error semget main handle_petent");
        cleanup();
        exit(1);
    }

    union semun arg;
    arg.val = 1;
    if (semctl(sems, 0, SETVAL, arg) == -1)
    {
        perror("rejestr semctl");
        log_msg("error semctl SETVAL handle_petent");
        cleanup();
        exit(1);
    }

    // dostajemy sie do kolejki
    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1)
    {
        perror("rejestr msgget");
        log_msg("error msgget handle_petent");
        exit(1);
    }

    int shm_id = shmget(key, sizeof(long), IPC_CREAT | 0666);
    if (shm_id == -1)
    {
        perror("rejestr shmget 3");
        log_msg("error shmget handle_petent");
        cleanup();
        exit(1);
    }

    int shm_id_2 = shmget(key_tabx, sizeof(int) * 5, 0);
    if (shm_id_2 == -1)
    {
        perror("rejestr shmget 4");
        log_msg("error shmget tabx handle_petent");
        cleanup();
        exit(1);
    }

    long *shared_mem = (long *)shmat(shm_id, NULL, 0);
    if (shared_mem == (long *)-1)
    {
        perror("rejestr shmat");
        log_msg("error shmat handle_petent");
        cleanup();
        exit(1);
    }

    int *tabx = (int *)shmat(shm_id_2, NULL, 0);
    if (tabx == (int *)-1)
    {
        perror("rejestr shmat");
        log_msg("error shmat tabx handle_petent");
        cleanup();
        exit(1);
    }

    *shared_mem = 0;

    pid_t rejestry[3] = {-1, -1, -1};
    rejestry[0] = getpid();

    int n = 0;
    log_msg("rejestr zaczyna glowna petle");
    while (1)
    {
        char message[50];
        sprintf(message, "wartosc CLOSE=%d", CLOSE);
        log_msg(message);
        if (CLOSE)
        {
            cleanup();
            exit(0);
        }

        check_petenci(pid[6], pid[5], key, shared_mem, rejestry, pid[7], pid);
        log_msg("rejestr odbiera wiadomosc");

        struct msgbuf_rejestr msg;
        int ret;

        // sprawdzamy czy są wiadomości VIP
        ret = msgrcv(msgid, &msg, sizeof(pid_t), 2, IPC_NOWAIT);
        if (ret == -1)
        {
            if (errno != ENOMSG)
            {
                if (errno == EINTR)
                    continue;
                perror("rejestr msgrcv VIP");
                cleanup();
                exit(1);
            }

            // czekamy na normalne wiadomości
            ret = msgrcv(msgid, &msg, sizeof(pid_t), 1, 0);
            if (ret == -1)
            {
                if (errno == EINTR) // jeśli czekanie przerywa sygnał, powtarzamy pętlę
                    continue;
                perror("rejestr msgrcv normal");
                cleanup();
                exit(1);
            }
        }

        pid_t temp = msg.pid;
        sprintf(message, "rejestr odebral wiadomosc od pid=%d", msg.pid);
        log_msg(message);
        msg.mtype = temp;

        // wybieramy, do którego urzędnika wysłać petenta
        int i = choose_pid(sems_2, tabx);
        sprintf(message, "rejestr wybral i=%d", i);
        log_msg(message);

        msg.pid = pid[i];
        sprintf(message, "rejestr wybral pid=%d", pid[i]);
        log_msg(message);
        log_msg("rejestr wysyla wiadomosc");
        msgsnd(msgid, &msg, sizeof(pid_t), 0);
    }
}

void handle_petent_klon(int pid[])
{
    log_msg("rejestr uruchamia handle_petent_klon");
    // tworzymy klucz z maską pid rejestru
    key_t key = ftok(".", getpid());
    if (key == -1)
    {
        perror("rejestr klon ftok");
        log_msg("error ftok handle_petent_klon");
        exit(1);
    }

    key_t key_tabx = ftok(".", 2);
    if (key_tabx == -1)
    {
        perror("rejestr klon ftok");
        log_msg("error ftok tabx handle_petent_klon");
        exit(1);
    }

    key_t key_main = ftok(".", 1);
    if (key_tabx == -1)
    {
        perror("rejestr klon ftok");
        log_msg("error ftok main handle_petent_klon");
        exit(1);
    }

    // tworzymy semafor do liczby czekających
    int sems = semget(key, 1, IPC_CREAT | 0666);
    if (sems == -1)
    {
        perror("rejestr klon semget");
        log_msg("error semget handle_petent_klon");
        cleanup_klon();
        exit(1);
    }

    int sems_2 = semget(key_main, ILE_SEMAFOROW, 0);
    if (sems_2 == -1)
    {
        perror("rejestr klon semget");
        log_msg("error main handle_petent_klon");
        cleanup_klon();
        exit(1);
    }

    union semun arg;
    arg.val = 1;
    if (semctl(sems, 0, SETVAL, arg) == -1)
    {
        perror("rejestr klon semctl");
        log_msg("error semctl SETVAL handle_petent_klon");
        cleanup_klon();
        exit(1);
    }

    // dostajemy sie do kolejki
    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1)
    {
        perror("rejestr klon msgget");
        log_msg("error msgget handle_petent_klon");
        cleanup_klon();
        exit(1);
    }

    int shm_id = shmget(key, sizeof(long), IPC_CREAT | 0666);
    if (shm_id == -1)
    {
        perror("rejestr klon shmget 5");
        log_msg("error shmget handle_petent_klon");
        cleanup_klon();
        exit(1);
    }

    int shm_id_2 = shmget(key_tabx, sizeof(int) * 5, 0);
    if (shm_id_2 == -1)
    {
        perror("rejestr klon shmget 6");
        log_msg("error shmget tabx handle_petent_klon");
        cleanup_klon();
        exit(1);
    }

    long *shared_mem = (long *)shmat(shm_id, NULL, 0);
    if (shared_mem == (long *)-1)
    {
        perror("rejestr klon shmat");
        log_msg("error shmat handle_petent_klon");
        cleanup_klon();
        exit(1);
    }

    int *tabx = (int *)shmat(shm_id_2, NULL, 0);
    if (tabx == (int *)-1)
    {
        perror("rejestr klon shmat");
        log_msg("error shmat tabx handle_petent_klon");
        cleanup_klon();
        exit(1);
    }

    *shared_mem = 0;

    int n = 0;
    log_msg("rejestr uruchamia glowna petle");
    while (1)
    {
        char message[50];
        sprintf(message, "wartosc CLOSE=%d", CLOSE);
        log_msg(message);
        struct msgbuf_rejestr msg;
        int ret;
        msg.mtype = 1;
        if (CLOSE)
        {
            log_msg("rejestr czysci kolejke i sie zamyka");
            while (msgrcv(msgid, &msg, sizeof(pid_t), 2, IPC_NOWAIT) != -1)
            {
                msg.mtype = msg.pid;
                int i = choose_pid(sems_2, tabx);

                msg.pid = pid[i];
                msgsnd(msgid, &msg, sizeof(pid_t), IPC_NOWAIT);
            }
            while (msgrcv(msgid, &msg, sizeof(pid_t), 1, IPC_NOWAIT) != -1)
            {
                msg.mtype = msg.pid;
                int i = choose_pid(sems_2, tabx);

                msg.pid = pid[i];
                msgsnd(msgid, &msg, sizeof(pid_t), IPC_NOWAIT);
            }

            cleanup_klon();
            exit(0);
        }
        log_msg("rejestr odbiera wiadomosc");

        // sprawdzamy czy są wiadomości VIP
        ret = msgrcv(msgid, &msg, sizeof(pid_t), 2, IPC_NOWAIT);
        if (ret == -1)
        {
            if (errno != ENOMSG)
            {
                if (errno == EINTR)
                    continue;
                perror("rejestr msgrcv VIP");
                cleanup_klon();
                exit(1);
            }

            // czekamy na normalne wiadomości
            ret = msgrcv(msgid, &msg, sizeof(pid_t), 1, 0);
            if (ret == -1)
            {
                if (errno == EINTR) // jeśli czekanie przerywa sygnał, powtarzamy pętlę
                    continue;
                perror("rejestr msgrcv normal");
                cleanup_klon();
                exit(1);
            }
        }
        pid_t temp = msg.pid;
        msg.mtype = temp;
        sprintf(message, "rejestr odebral wiadomosc od pid=%d", msg.pid);
        log_msg(message);

        int i = choose_pid(sems_2, tabx);
        sprintf(message, "rejestr wybral i=%d", i);
        log_msg(message);

        msg.pid = pid[i];
        sprintf(message, "rejestr wybral pid=%d", pid[i]);
        log_msg(message);
        log_msg("rejestr wysyla wiadomosc");
        msgsnd(msgid, &msg, sizeof(pid_t), 0);
    }
}

void cleanup()
{
    log_msg("rejestr uruchamia cleanup");
    key_t key = ftok(".", getpid());
    if (key == -1)
        perror("rejestr ftok cleanup");

    key_t key_tabx = ftok(".", 2);
    if (key_tabx == -1)
        perror("rejestr ftok cleanup");

    // dostajemy sie do kolejki
    int msgid = msgget(key, 0);
    if (msgid == -1)
        perror("rejestr msgget cleanup");

    if (msgctl(msgid, IPC_RMID, NULL) == -1) // TODO: jeśli kolejki już nie ma, zwróci -1, mimo że to nas nie obchodzi
        perror("rejestr msgctl cleanup");

    int shmid = shmget(key, sizeof(long), 0);
    if (shmid != -1)
        shmctl(shmid, IPC_RMID, NULL);
    else
        perror("rejestr shmctl cleanup");

    int semid = semget(key, 1, 0);
    if (semid != -1)
        semctl(semid, 0, IPC_RMID);
    else
        perror("rejestr semctl cleanup");

    shmid = shmget(key_tabx, sizeof(int) * 5, 0);
    if (shmid != -1)
        shmctl(shmid, IPC_RMID, NULL);
    else
        perror("rejestr schmctl cleanup");

    fclose(f);
}

void cleanup_klon()
{
    log_msg("rejestr uruchamia cleanup_klon");
    key_t key = ftok(".", getpid());
    if (key == -1)
        perror("rejestr klon ftok cleanup");

    // dostajemy sie do kolejki
    int msgid = msgget(key, 0);
    if (msgid == -1)
        perror("rejestr klon msgget cleanup");

    if (msgctl(msgid, IPC_RMID, NULL) == -1) // TODO: jeśli kolejki już nie ma, zwróci -1, mimo że to nas nie obchodzi
        perror("rejestr klon msgctl cleanup");

    int shmid = shmget(key, sizeof(long), 0);
    if (shmid != -1)
        shmctl(shmid, IPC_RMID, NULL);
    else
        perror("rejestr klon shmctl cleanup");

    int semid = semget(key, 1, 0);
    if (semid != -1)
        semctl(semid, 0, IPC_RMID);
    else
        perror("rejestr klon semctl cleanup");

    semid = semget(key, 0, 0);
    if (semid != -1)
        semctl(semid, 0, IPC_RMID);
    else
        perror("rejestr klon semctl cleanup");
    fclose(f);
}

void check_petenci(int N, int K, key_t key, long *shared_mem, pid_t pid[], pid_t pid_generator, int tab[])
{
    log_msg("rejestr uruchamia check_petenci");
    if (getpid() != pid[0])
        return; // tylko główny rejestr steruje tworzeniem nowych rejestrów

    struct sembuf P = {.sem_num = 0, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = 0, .sem_op = +1, .sem_flg = 0};

    int zmieniono = 0;

    // Pobranie semafora głównego rejestru
    int sems = semget(key, 1, 0);
    if (sems == -1)
    {
        perror("rejestr semget");
        log_msg("error semget check_petenci");
        cleanup();
        exit(1);
    }

    log_msg("rejestr blokuje semafor 0 self");
    semop(sems, &P, 1);

    printf("aktywnych petentow - %ld\n", *shared_mem);
    char message[50];
    sprintf(message, "aktywnych petentow=%ld", *shared_mem);
    log_msg(message);

    // tworzymy rejestr 1
    if (*shared_mem >= K && pid[1] <= 0)
    {
        printf("otwarto nowy rejestr\n");
        log_msg("otwieramy nowy rejestr");
        pid_t temp = fork();
        if (temp == -1)
        {
            perror("rejestr fork");
            log_msg("error fork check_petenci");
            cleanup();
            exit(1);
        }

        if (temp > 0)
        {
            // Rodzic zapisuje PID dziecka
            pid[1] = temp;
            zmieniono = 1;
            kill(pid_generator, SIGRTMIN);
            sprintf(message, "rejestr wyslal SIGRTMIN do generator pid=%d", pid_generator);
            log_msg(message);
        }
        else
        {
            srand(time(NULL) ^ getpid());
            f = fopen("./Logi/rejestr_1", "a");
            if (!f)
            {
                perror("rejestr fopen");
                cleanup_klon();
                exit(1);
            }
            sprintf(message, "rejestr uruchomiony pid=%d", getpid());
            log_msg(message);

            handle_petent_klon(tab);

            cleanup_klon();
            log_msg("rejestr sie zamyka");
            exit(0);
        }
    }

    // zamykamy rejestr 1
    if (*shared_mem < N / 3 && pid[1] > 0)
    {
        printf("zamknieto rejestr\n");
        log_msg("zamykanie rejestru");
        kill(pid_generator, SIGRTMIN); // najpierw generator, zeby nie wysylal do nieistniejących procesów
        sprintf(message, "rejestr wyslal SIGRTMIN do generator pid=%d", pid_generator);
        log_msg(message);
        kill(pid[1], SIGINT);
        log_msg("rejestr wyslal SIGINT do rejestru");
        waitpid(pid[1], NULL, 0);
        log_msg("rejestr sie zamkna");
        pid[1] = -1;
        zmieniono = 1;
    }

    // tworzymy rejestr 2
    if (*shared_mem >= 2 * K && pid[2] <= 0)
    {
        printf("otwarto nowy rejestr\n");
        log_msg("otwieramy nowy rejestr");
        pid_t temp = fork();
        if (temp < 0)
        {
            perror("rejestr fork");
            log_msg("error fork check_petenci");
            return;
        }

        if (temp > 0)
        {
            pid[2] = temp;
            zmieniono = 1;
            kill(pid_generator, SIGRTMIN);
            sprintf(message, "rejestr wyslal SIGRTMIN do generator pid=%d", pid_generator);
            log_msg(message);
        }
        else
        {
            srand(time(NULL) ^ getpid());
            f = fopen("./Logi/rejestr_2", "a");
            if (!f)
            {
                perror("rejestr fopen");
                cleanup_klon();
                exit(1);
            }
            sprintf(message, "rejestr uruchomiony pid=%d", getpid());
            log_msg(message);

            handle_petent_klon(tab);

            cleanup_klon();
            log_msg("rejestr sie zamyka");
            exit(0);
        }
    }

    // zamykamy rejestr 2
    if (*shared_mem < (2 * N) / 3 && pid[2] > 0)
    {
        printf("zamknieto rejestr\n");
        log_msg("zamykanie rejestru");
        kill(pid_generator, SIGRTMIN);
        sprintf(message, "rejestr wyslal SIGRTMIN do generator pid=%d", pid_generator);
        log_msg(message);
        kill(pid[2], SIGINT);
        log_msg("rejestr wyslal SIGINT do rejestru");
        waitpid(pid[2], NULL, 0);
        log_msg("rejestr sie zamkna");
        pid[2] = -1;
        zmieniono = 1;
    }

    log_msg("rejestr oddaje semafor 0 self");
    semop(sems, &V, 1);

    if (zmieniono)
        send_generator(pid, pid_generator);
}

void send_generator(pid_t pid[], pid_t pid_generator)
{
    log_msg("rejestr uruchamia send_generator");
    key_t key = ftok(".", 1);
    if (key == -1)
    {
        perror("rejestr ftok");
        log_msg("error ftok send_generator");
        exit(1);
    }

    int sems = semget(key, ILE_SEMAFOROW, 0);
    if (sems == -1)
    {
        perror("rejestr semget");
        log_msg("error semget send_generator");
        cleanup();
        exit(1);
    }

    int shm_id = shmget(key, sizeof(key_t), 0);
    if (shm_id == -1)
    {
        perror("rejestr shmget 7");
        log_msg("error shmget send_generator");
        cleanup();
        exit(1);
    }

    key_t *shared_mem = (key_t *)shmat(shm_id, NULL, 0);
    if (shared_mem == (key_t *)-1)
    {
        perror("rejestr shmat");
        log_msg("error shmat send_generator");
        cleanup();
        exit(1);
    }

    struct sembuf P = {.sem_num = SEMAFOR_REJESTR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_GENERATOR, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < 3; i++)
    {
        log_msg("rejestr blokuje semafor REJESTR");
        while (semop(sems, &P, 1) == -1) // czekamy czy można wysyłać
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("rejestr semop P");
                log_msg("error semop send_generator");
                cleanup();
                exit(1);
            }
        }

        *shared_mem = pid[i];

        log_msg("rejestr oddaje semafor GENERATOR");
        while (semop(sems, &V, 1) == -1) // zaznaczamy, że można czytać
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("rejestr semop V");
                log_msg("error semop send_generator");
                cleanup();
                exit(1);
            }
        }
    }

    shmdt(shared_mem);
}

void log_msg(char *msg)
{
    t = time(NULL);
    t_broken = localtime(&t);
    fprintf(f, "<%02d:%02d:%02d> %s\n", t_broken->tm_hour, t_broken->tm_min, t_broken->tm_sec, msg);
    fflush(f);
}