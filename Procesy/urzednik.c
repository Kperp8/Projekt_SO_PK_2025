#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

FILE *f;
time_t t;
struct tm *t_broken;

volatile sig_atomic_t CLOSE_GENTLY = 0;
volatile sig_atomic_t FORCE_EXIT = 0;

int typ;

struct msgbuf_urzednik // wiadomość od urzednika
{
    long mtype;
    char mtext[30];
    pid_t pid;
} __attribute__((packed));

void SIGUSR1_handle(int sig);
void SIGUSR2_handle(int sig);
void SIGINT_handle(int sig);
void EMPTY_handle(int sig);
void install_handler(int signo, void (*handler)(int));

void handle_petent();
void cleanup();
void log_msg(char *msg);

int main(int argc, char **argv)
{
    install_handler(SIGUSR1, SIGUSR1_handle);
    install_handler(SIGUSR2, SIGUSR2_handle);
    install_handler(SIGINT, SIGINT_handle);
    install_handler(SIGRTMIN, EMPTY_handle);
    srand(time(NULL));
    printf("urzednik\n");
    raise(SIGSTOP);

    key_t key = atoi(argv[1]);
    typ = atoi(argv[2]);

    char filename[50];
    sprintf(filename, "./Logi/urzednik_%d", typ);
    f = fopen(filename, "w");
    if (!f)
    {
        perror("urzednik fopen");
        cleanup();
        exit(1);
    }
    log_msg("urzednik uruchomiony");

    handle_petent();

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
    (void)sig;
    CLOSE_GENTLY = 1;
}

void SIGUSR2_handle(int sig)
{
    (void)sig;
    FORCE_EXIT = 1;
}

void SIGINT_handle(int sig)
{
    (void)sig;
    FORCE_EXIT = 1;
}

void EMPTY_handle(int sig)
{
}

void handle_petent(void)
{
    key_t key = (typ == 5) ? ftok(".", getpid() - 1) : ftok(".", getpid());
    if (key == -1)
    {
        perror("ftok");
        exit(1);
    }

    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1)
    {
        perror("msgget");
        exit(1);
    }

    log_msg("urzednik start petli");

    while (1)
    {
        if (FORCE_EXIT)
        {
            log_msg("urzednik FORCE_EXIT");
            cleanup();
            exit(0);
        }

        struct msgbuf_urzednik msg;
        int ret;

        if (CLOSE_GENTLY)
        {
            log_msg("urzednik CLOSE_GENTLY");

            while (1)
            {
                ret = msgrcv(msgid, &msg,
                             sizeof(msg) - sizeof(long),
                             0, IPC_NOWAIT);

                if (ret == -1)
                {
                    if (errno == ENOMSG)
                        break;
                    if (errno == EINTR)
                        continue;
                    perror("msgrcv");
                    break;
                }

                msg.mtype = msg.pid;
                msg.pid = -1;
                snprintf(msg.mtext, sizeof(msg.mtext), "jestes przetworzony\n");
                msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0);
            }

            cleanup();
            exit(0);
        }

        ret = msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), 2, IPC_NOWAIT);
        if (ret == -1)
        {
            if (errno != ENOMSG)
            {
                if (errno == EINTR)
                    continue;
                perror("msgrcv VIP");
                break;
            }

            ret = msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), 1, 0);
            if (ret == -1)
            {
                if (errno == EINTR)
                    continue;
                perror("msgrcv normal");
                break;
            }
        }

        pid_t pid = msg.pid;
        msg.mtype = pid;

        if (typ < 4)
        {
            if (rand() % 10 == 1)
            {
                strcpy(msg.mtext, "prosze udac sie do kasy\n");
                msg.pid = 12;
            }
            else
            {
                strcpy(msg.mtext, "jestes przetworzony\n");
                msg.pid = -1;
            }
        }
        else
        {
            if (rand() % 10 < 4)
            {
                strcpy(msg.mtext, "prosze udac sie do urzednika\n");
                msg.pid = typ == 4 ? getpid() - rand() % 4 - 1 : getpid() - rand() % 4 - 2;
            }
            else
            {
                strcpy(msg.mtext, "jestes przetworzony\n");
                msg.pid = -1;
            }
        }

        msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0);
    }

    cleanup();
}

void cleanup()
{
    if (typ == 5)
    {
        fclose(f);
        return;
    }
    log_msg("urzednik uruchamia cleanup");
    key_t key = ftok(".", getpid());
    if (key == -1)
        perror("urzednik ftok");

    int msgid = msgget(key, 0);
    if (msgid == -1)
        perror("urzednik msgget");

    if (msgctl(msgid, IPC_RMID, NULL) == -1)
        perror("urzednik msgctl");
    fclose(f);
}

void log_msg(char *msg)
{
    t = time(NULL);
    t_broken = localtime(&t);
    fprintf(f, "<%02d:%02d:%02d> %s\n", t_broken->tm_hour, t_broken->tm_min, t_broken->tm_sec, msg);
    fflush(f);
}