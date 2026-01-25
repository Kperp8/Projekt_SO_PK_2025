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

int CLOSE = 0;

int tab_X[5] = {
    10, // X1
    10, // X2
    10, // X3
    10, // X4
    10, // X5
};

int typ;

struct msgbuf_urzednik // wiadomość od urzednika
{
    long mtype;
    char mtext[30]; // TODO: przemyslec jak ma wygladac wiadomosc od urzednika
    pid_t pid;
} __attribute__((packed));

void SIGUSR1_handle(int sig);
void SIGUSR2_handle(int sig);
void SIGINT_handle(int sig);

void handle_petent();
void cleanup();

int main(int argc, char **argv)
{
    signal(SIGUSR1, SIGUSR1_handle);
    signal(SIGUSR2, SIGUSR2_handle);
    signal(SIGINT, SIGINT_handle);
    srand(time(NULL));
    printf("urzednik\n");

    key_t key = atoi(argv[1]);
    typ = atoi(argv[2]);

    handle_petent();

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
    CLOSE = 1;
}

void handle_petent()
{
    // tworzymy klucz z maską pid rejestru
    key_t key = typ == 5 ? ftok(".", getpid() - 1) : ftok(".", getpid());
    if (key == -1)
    {
        perror("urzednik ftok");
        exit(1);
    }

    // dostajemy sie do kolejki
    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1)
    {
        perror("urzednik msgget");
        exit(1);
    }

    int n = 0;
    while (1)
    {
        if (CLOSE)
        {
            cleanup();
            exit(0);
        }

        struct msgbuf_urzednik msg;
        msg.mtype = 1;
        msgrcv(msgid, &msg, sizeof(struct msgbuf_urzednik) - sizeof(long), 1, 0); // TODO: obsłużyć jeśli kolejka pusta itd.
        pid_t temp = msg.pid;
        msg.mtype = temp;
        if (typ < 4)
        {
            int l = rand() % 10;
            if (l == 1)
            {
                sprintf(msg.mtext, "%s", "prosze udac sie do kasy\n");
                msg.pid = 12; // placeholder
            }
            else
            {
                sprintf(msg.mtext, "%s", "jestes przetworzony\n");
                msg.pid = -1;
            }
        }
        else
        {
            int l = rand() % 10;
            if (l < 4)
            {
                sprintf(msg.mtext, "%s", "prosze udac sie do urzednika\n");
                msg.pid = typ == 4 ? getpid() - rand() % 4 - 1 : rand() % 4 - 2;
            }
            else
            {
                sprintf(msg.mtext, "%s", "jestes przetworzony\n");
                msg.pid = -1;
            }
        }
        msgsnd(msgid, &msg, sizeof(struct msgbuf_urzednik) - sizeof(long), 0);
    }
}

void cleanup()
{
    key_t key = ftok(".", getpid());
    if (key == -1)
    {
        perror("urzednik ftok");
        exit(1);
    }

    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1)
    {
        perror("urzednik msgget");
        exit(1);
    }

    if (msgctl(msgid, IPC_RMID, NULL) == -1)
    {
        perror("urzednik msgctl");
        exit(1);
    }
}