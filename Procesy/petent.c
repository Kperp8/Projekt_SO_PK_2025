#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

// TODO: obsługa sygnałów SIGINT, 1, 2

struct msgbuf_rejestr
{
    long mtype;
    pid_t pid;
} __attribute__((packed));

struct msgbuf_urzednik
{
    long mtype;
    char mtext[20]; // TODO: przemyslec jak ma wygladac wiadomosc od urzednika
} __attribute__((packed));

pid_t recieve_rejestr(pid_t r_pid);
void handle_urzednik(pid_t u_pid);

int main(int argc, char **argv)
{
    printf("%s %s %d pid %d\n", argv[2], argv[3], atoi(argv[4]), getpid());
    // idziemy do rejestru
    pid_t r_pid = atoi(argv[4]);
    // odbieramy pid urzednika
    pid_t u_pid = recieve_rejestr(r_pid);
    // idziemy do urzednika
    handle_urzednik(u_pid);
    return 0;
}

pid_t recieve_rejestr(pid_t r_pid)
{
    // tworzymy klucz z maską pid rejestru
    key_t key = ftok("..", r_pid);
    if (key == -1)
    {
        perror("klient ftok");
        exit(1);
    }

    // dostajemy sie do kolejki
    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1)
    {
        perror("petent msgget");
        exit(1);
    }

    struct msgbuf_rejestr msg;
    msg.mtype = 1;
    return msgrcv(msgid, &msg, sizeof(pid_t), 1, 0); // TODO: obsłużyć jeśli kolejka pusta itd.
}

void handle_urzednik(pid_t u_pid)
{
    // tworzymy klucz z maską pid rejestru
    key_t key = ftok("..", u_pid);
    if (key == -1)
    {
        perror("klient ftok");
        exit(1);
    }

    // dostajemy sie do kolejki
    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1)
    {
        perror("petent msgget");
        exit(1);
    }

    struct msgbuf_urzednik msg;
    msg.mtype = 1;
    msgrcv(msgid, &msg, sizeof(pid_t), 1, 0); // TODO: obsłużyć jeśli kolejka pusta itd.
}