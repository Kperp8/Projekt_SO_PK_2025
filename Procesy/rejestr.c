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
#define ILE_SEMAFOROW 9

// bedzie można użyć pipe() przy tworzeniu kolejnych rejestrów

// TODO: powielanie rejestru i wysyłanie pidów kopii

// TODO: pomyśleć, czy lepiec tego nie odbierać od kogoś innego, np main
int tab_X[5] = {
    10, // X1
    10, // X2
    10, // X3
    10, // X4
    10, // X5
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

int recieve_dyrektor(int sems, key_t *shared_mem, int result[]);
void handle_petent(int pid[]);
pid_t give_bilet();
void check_petenci(int N, int K, key_t key, int msgid, int *shared_mem, pid_t pid[], pid_t pid_generator); // sprawdza ile jest petentow w kolejce, otwiera nowe procesy rejestr
void send_generator(pid_t pid[], pid_t pid_generator);
void cleanup();

int main(int argc, char **argv)
{
    signal(SIGUSR1, SIGUSR1_handle);
    signal(SIGUSR2, SIGUSR2_handle);
    signal(SIGINT, SIGINT_handle);
    srand(time(NULL));
    printf("rejestr\n");

    key_t key;
    key = atoi(argv[1]);

    int sems = semget(key, ILE_SEMAFOROW, 0); // semafory
    if (sems == -1)
    {
        perror("rejestr semget");
        exit(1);
    }

    int shm_id = shmget(key, sizeof(key_t), 0); // pamiec
    if (shm_id == -1)
    {
        perror("rejestr shmget");
        exit(1);
    }

    key_t *shared_mem = (key_t *)shmat(shm_id, NULL, 0); // podlaczamy pamiec
    if (shared_mem == (key_t *)-1)
    {
        perror("rejestr shmat");
        exit(1);
    }

    key_t tab[8]; // tab[0-4] - p_id, tab[5] - K, tab[6] - N, tab[7] - p_id[7]
    if (recieve_dyrektor(sems, shared_mem, tab) != 0)
    {
        perror("rejestr recieve dyrektor");
        exit(1); // TODO: tu jest problem, nie ma mechanizmu do wykraczania jeśli podproces się wykraczy
    }

    printf("rejestr: otrzymano N=%d, K=%d, ", tab[6], tab[5]);
    for (int i = 0; i < 5; i++)
        printf("pid[%d]=%d, ", i, tab[i]);
    printf("\n");

    handle_petent(tab);

    cleanup();
    return 0;
}

void SIGUSR1_handle(int sig)
{
    cleanup();
}

void SIGUSR2_handle(int sig)
{
    cleanup();
}

void SIGINT_handle(int sig)
{
    cleanup();
}

int recieve_dyrektor(int sems, key_t *shared_mem, int result[])
{
    struct sembuf P = {.sem_num = SEMAFOR_REJESTR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < 8; i++)
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

        result[i] = (int)*shared_mem;

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

void handle_petent(int pid[])
{
    // tworzymy klucz z maską pid rejestru
    key_t key = ftok(".", getpid());
    if (key == -1)
    {
        perror("rejestr ftok");
        exit(1);
    }

    // dostajemy sie do kolejki
    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1)
    {
        perror("rejestr msgget");
        exit(1);
    }

    int shm_id = shmget(key, sizeof(key_t), 0); // pamiec
    if (shm_id == -1)
    {
        perror("rejestr shmget");
        cleanup();
        exit(1);
    }

    int *shared_mem = (int *)shmat(shm_id, NULL, 0); // podlaczamy pamiec
    if (shared_mem == (int *)-1)
    {
        perror("rejestr shmat");
        cleanup();
        exit(1);
    }

    pid_t rejestry[3];
    rejestry[0] = getpid();

    int n = 0;
    while (n++ < 20) // TODO: poprawic na while(1), to jest test
    {
        struct msgbuf_rejestr msg;
        msg.mtype = 1;
        check_petenci(pid[6], pid[5], key, msgid, shared_mem, rejestry, pid[7]);
        msgrcv(msgid, &msg, sizeof(pid_t), 0, 0); // TODO: obsłużyć jeśli kolejka pusta itd.
        pid_t temp = msg.pid;
        msg.mtype = temp;

        // losujemy pid
        int i = rand() % 10;
        i = i < 4 ? i : 4; // TODO: narazie nie ma drugiego urzędnika SA

        msg.pid = pid[i];
        msgsnd(msgid, &msg, sizeof(pid_t), 0);
    }
}

void cleanup()
{
    key_t key = ftok(".", getpid());
    if (key == -1)
    {
        perror("rejestr ftok");
        exit(1);
    }

    // dostajemy sie do kolejki
    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1)
    {
        perror("rejestr msgget");
        exit(1);
    }

    if (msgctl(msgid, IPC_RMID, NULL) == -1)
    {
        perror("rejestr msgctl");
        exit(1);
    }
}

void check_petenci(int N, int K, key_t key, int msgid, int *shared_mem, pid_t pid[], pid_t pid_generator)
{
    // TODO: niepotrzebnie dużo wywołań getpid()
    // logika nowych procesów
    int zmieniono = 0;
    if (*shared_mem >= K)
    {
        pid_t temp = fork();
        if (temp == -1)
        {
            perror("rejestr fork");
            cleanup(); // TODO: chyba nie trzeba zawalać
            exit(1);
        }
        pid[1] = temp;
        zmieniono = 1;
    }

    if (getpid() == pid[0])
        if (*shared_mem < N / 3)
        {
            kill(pid[1], SIGKILL); // TODO: error handling
            zmieniono = 1;
        }

    if (getpid() == pid[0])
        if (*shared_mem >= 2 * K)
        {
            if (getpid() == pid[0])
            {
                pid_t temp = fork();
                if (temp == -1)
                {
                    perror("rejestr fork");
                    cleanup(); // TODO: chyba nie trzeba zawalać
                    exit(1);
                }
                pid[2] = temp;
                zmieniono = 1;
            }
        }

    if (getpid() == pid[0])
        if (*shared_mem < (2 / 3) * N)
        {
            kill(pid[2], SIGKILL); // TODO: error handling
            zmieniono = 1;
        }

    if (getpid() == pid[0])
        if (zmieniono)
            send_generator(pid, pid_generator);
}

void send_generator(pid_t pid[], pid_t pid_generator) // TODO: na razie troche brzydko, przemyśleć
{
    // musimy przesłać tablicę pid to generatora
    // wysyłamy sygnał SIGRTMIN do generatora
    kill(pid_generator, SIGRTMIN);
    // on w handlerze odbiera tablice
    key_t key = ftok(".", 1);
    if (key == -1)
    {
        perror("main - ftok");
        exit(1);
    }

    int sems = semget(key, ILE_SEMAFOROW, 0); // semafory
    if (sems == -1)
    {
        perror("dyrektor semget");
        cleanup();
        exit(1);
    }

    int shm_id = shmget(key, sizeof(key_t), 0); // pamiec
    if (shm_id == -1)
    {
        perror("dyrektor shmget");
        cleanup();
        exit(1);
    }

    key_t *shared_mem = (key_t *)shmat(shm_id, NULL, 0); // podlaczamy pamiec
    if (shared_mem == (key_t *)-1)
    {
        perror("dyrektor shmat");
        cleanup();
        exit(1);
    }

    union semun arg;
    arg.val = 1;
    if (semctl(sems, SEMAFOR_REJESTR, SETVAL, arg) == -1)
    {
        perror("rejestr semctl");
        cleanup();
        exit(1);
    }
    arg.val = 0;
    if (semctl(sems, SEMAFOR_GENERATOR, SETVAL, arg) == -1)
    {
        perror("rejestr semctl");
        cleanup();
        exit(1);
    }

    struct sembuf temp = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = -1, .sem_flg = 0};
    semop(sems, &temp, 1);

    struct sembuf P = {.sem_num = SEMAFOR_REJESTR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_GENERATOR, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < 3; i++)
    {
        while (semop(sems, &P, 1) == -1) // czekamy czy można wysyłać
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("rejestr semop P");
                cleanup();
                exit(1);
            }
        }

        *shared_mem = pid[i];

        while (semop(sems, &V, 1) == -1) // zaznaczamy, że można czytać
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("rejestr semop V");
                cleanup();
                exit(1);
            }
        }
    }
}