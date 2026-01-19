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

int tab_X[5] = {
    10, // X1
    10, // X2
    10, // X3
    10, // X4
    10, // X5
};

struct msgbuf_urzednik // wiadomość od urzednika
{
    long mtype;
    char mtext[20]; // TODO: przemyslec jak ma wygladac wiadomosc od urzednika
    pid_t pid;
} __attribute__((packed));

void SIGUSR1_handle(int sig);
void SIGUSR2_handle(int sig);

void handle_petent(key_t key);

int main(int argc, char **argv)
{
    signal(SIGUSR1, SIGUSR1_handle);
    signal(SIGUSR2, SIGUSR2_handle);
    printf("urzednik\n");

    key_t key = atoi(argv[1]);
    int rodzaj = atoi(argv[2]);

    handle_petent(key);
    return 0;
}

void SIGUSR1_handle(int sig)
{
    printf("urzednik przychwycil SIGUSR1\n");
}

void SIGUSR2_handle(int sig)
{
    printf("urzednik przychwycil SIGUSR2\n");
}

void handle_petent(key_t key)
{
    // tworzymy klucz z maską pid rejestru
    key_t key = ftok("..", getpid());
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
    while (n++ < 20) // TODO: poprawic na while(1), to jest test
    {
        struct msgbuf_urzednik msg;
        msg.mtype = 1;
        msgrcv(msgid, &msg, sizeof(struct msgbuf_urzednik) - sizeof(long), 1, 0); // TODO: obsłużyć jeśli kolejka pusta itd.
        pid_t temp = msg.pid;
        sprintf(msg.mtext, "%s", "jestes przetworzony\n"); // TODO: oczywiście wiadomość ma być zupełnie inna i zależna od rodzaju urzędnika
        msg.pid = getpid();
        msgsnd(msgid, &msg, sizeof(struct msgbuf_urzednik) - sizeof(long), temp);
    }
}