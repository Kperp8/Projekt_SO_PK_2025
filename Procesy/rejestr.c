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
void check_petenci(int N, int K, key_t key, int msgid, long *shared_mem, pid_t pid[], pid_t pid_generator); // sprawdza ile jest petentow w kolejce, otwiera nowe procesy rejestr
void send_generator(pid_t pid[], pid_t pid_generator);
void cleanup();

int main(int argc, char **argv)
{
    signal(SIGUSR1, SIGUSR1_handle);
    signal(SIGUSR2, SIGUSR2_handle);
    signal(SIGINT, SIGINT_handle);
    srand(time(NULL));
    printf("rejestr %d\n", getpid());

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

    key_t tab[8]; // tab[0-4] - p_id, tab[5] - K, tab[6] - N, tab[7] - p_id[7]
    if (recieve_dyrektor(sems, shared_mem, tab) != 0)
    {
        perror("rejestr recieve dyrektor");
        exit(1); // TODO: tu jest problem, nie ma mechanizmu do wykraczania jeśli podproces się wykraczy
    }
    shmdt(shared_mem);

    printf("rejestr: otrzymano N=%d, K=%d, pid[7]=%d, ", tab[6], tab[5], tab[7]);
    for (int i = 0; i < 5; i++)
        printf("pid[%d]=%d, ", i, tab[i]);
    printf("\n");

    handle_petent(tab);

    printf("rejestr sie zamyka\n");
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

    int shm_id = shmget(key, sizeof(long), IPC_CREAT | 0666); // pamiec
    if (shm_id == -1)
    {
        perror("rejestr shmget");
        cleanup();
        exit(1);
    }

    long *shared_mem = (long *)shmat(shm_id, NULL, 0); // podlaczamy pamiec
    if (shared_mem == (long *)-1)
    {
        perror("rejestr shmat");
        cleanup();
        exit(1);
    }

    *shared_mem = 0;

    pid_t rejestry[3];
    rejestry[0] = getpid();

    int n = 0;
    while (1) // TODO: poprawic na while(1), to jest test
    {
        struct msgbuf_rejestr msg;
        msg.mtype = 1;
        check_petenci(pid[6], pid[5], key, msgid, shared_mem, rejestry, pid[7]);
        msgrcv(msgid, &msg, sizeof(pid_t), 1, 0); // TODO: obsłużyć jeśli kolejka pusta itd.
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
    int msgid = msgget(key, 0);
    if (msgid == -1)
    {
        perror("rejestr msgget");
        exit(1);
    }

    if (msgctl(msgid, IPC_RMID, NULL) == -1) // TODO: jeśli kolejki już nie ma, zwróci -1, mimo że to nas nie obchodzi
    {
        perror("rejestr msgctl");
        exit(1);
    }

    int shmid = shmget(key, sizeof(int), 0);
    if (shmid != -1)
        shmctl(shmid, IPC_RMID, NULL);
}

void check_petenci(int N, int K, key_t key, int msgid,
                   long *shared_mem, pid_t pid[], pid_t pid_generator)
{
    if (getpid() != pid[0])
        return; // tylko główny rejestr

    int zmieniono = 0;

    /* === REJESTR 1 === */
    if (*shared_mem >= K && pid[1] <= 0)
    {
        printf("otwarto nowy rejestr\n");
        pid_t temp = fork();
        if (temp < 0)
        {
            perror("rejestr fork");
            exit(1);
        }
        if (temp > 0)
        {
            pid[1] = temp;
            zmieniono = 1;
        }
        if (temp == 0)
            return;
    }

    if (*shared_mem < N / 3 && pid[1] > 0)
    {
        printf("zamknieto rejestr\n");
        kill(pid[1], SIGKILL);
        waitpid(pid[1], NULL, 0);
        pid[1] = -1;
        zmieniono = 1;
    }

    /* === REJESTR 2 === */
    if (*shared_mem >= 2 * K && pid[2] <= 0)
    {
        printf("otwarto nowy rejestr\n");
        pid_t temp = fork();
        if (temp < 0)
        {
            perror("rejestr fork");
            return;
        }
        if (temp > 0)
        {
            pid[2] = temp;
            zmieniono = 1;
        }
        if (temp == 0)
            return;
    }

    if (*shared_mem < (2 * N) / 3 && pid[2] > 0)
    {
        printf("zamknieto rejestr\n");
        kill(pid[2], SIGKILL);
        waitpid(pid[2], NULL, 0);
        pid[2] = -1;
        zmieniono = 1;
    }

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
        perror("rejestr ftok");
        exit(1);
    }

    int sems = semget(key, ILE_SEMAFOROW, 0); // semafory
    if (sems == -1)
    {
        perror("rejestr semget");
        cleanup();
        exit(1);
    }

    int shm_id = shmget(key, sizeof(key_t), 0); // pamiec
    if (shm_id == -1)
    {
        perror("rejestr shmget");
        cleanup();
        exit(1);
    }

    key_t *shared_mem = (key_t *)shmat(shm_id, NULL, 0); // podlaczamy pamiec
    if (shared_mem == (key_t *)-1)
    {
        perror("rejestr shmat");
        cleanup();
        exit(1);
    }

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

    shmdt(shared_mem);
}