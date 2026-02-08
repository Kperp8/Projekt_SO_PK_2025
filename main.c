#include "Procesy/common.h"

/*
0-5: urzednicy
6: rejestr
7: generator
8: dyrektor
*/
pid_t p_id[ILE_PROCESOW]; // tablica pid procesow potomnych
key_t key;

FILE *f;
time_t t;
struct tm *t_broken;

void SIGINT_handle(int sig);
void EMPTY_handle(int sig);

// usuwa semafory, pamiec wspoldzielona i zamyka wszystkie procesy
void cleanup();
void start_procesy();
void init_sem(int sems);
void send_dyrektor(int sems, key_t *shared_mem);
void log_msg(char *msg);

int main(int argc, char **argv)
{
    signal(SIGINT, SIGINT_handle);
    signal(SIGUSR1, EMPTY_handle);
    signal(SIGUSR2, EMPTY_handle);

    f = fopen("./Logi/main", "w");
    if (!f)
    {
        perror("main fopen");
        cleanup();
        exit(1);
    }
    log_msg("main uruchomiony");

    // tworzymy klucz
    key = ftok(".", 1);
    if (key == -1)
    {
        perror("main - ftok");
        log_msg("error ftok main");
        exit(1);
    }

    // tworzymy semafory
    int sems = semget(key, ILE_SEMAFOROW, IPC_CREAT | 0666);
    if (sems == -1)
    {
        perror("main - semget");
        log_msg("error semget main");
        cleanup();
        exit(1);
    }

    // tworzymy pamiec dzieloną
    int shmid = shmget(key, sizeof(pid_t), IPC_CREAT | 0666);
    if (shmid == -1)
    {
        perror("main shmget");
        log_msg("error shmget main");
        cleanup();
        exit(1);
    }

    pid_t *shared_mem = (pid_t *)shmat(shmid, NULL, 0);
    if (shared_mem == (pid_t *)-1)
    {
        perror("main shmat");
        log_msg("error shmat");
        cleanup();
        exit(1);
    }

    start_procesy();

    init_sem(sems);

    send_dyrektor(sems, shared_mem);

    shmdt(shared_mem);

    log_msg("main czeka");
    for (int i = 0; i < ILE_PROCESOW; i++)
        waitpid(p_id[i], NULL, 0);

    return 0;
}

void cleanup()
{
    log_msg("main wykonuje cleanup");
    fclose(f);
}

void SIGINT_handle(int sig)
{
    log_msg("main przechwycil SIGINT");
    cleanup();
    exit(0);
}

void EMPTY_handle(int sig)
{
}

void start_procesy()
{
    int n = -1;
    char key_str[sizeof(key_t) * 8];
    sprintf(key_str, "%d", key);

    // uruchamiamy procesy urzednik
    log_msg("main uruchamia urzednik");
    for (int i = 0; i < 6; i++)
    {
        p_id[++n] = fork();
        if (p_id[n] == 0)
        {
            char u_id[2]; // dodatkowy identyfikator rodzaju urzednika
            sprintf(u_id, "%d", i);
            execl("Procesy/urzednik", "Procesy/urzednik", key_str, u_id, NULL);
            perror("main - execl urzednik");
            log_msg("error execl urzednik");
            cleanup();
            exit(1);
        }
    }
    log_msg("main uruchomil urzednik");

    // uruchamiamy procesy Rejestracja
    log_msg("main uruchamia rejestr");
    p_id[++n] = fork();
    if (p_id[n] == 0)
    {
        execl("Procesy/rejestr", "Procesy/rejestr", key_str, NULL);
        perror("main - execl rejestr");
        log_msg("error execl rejestr");
        cleanup();
        exit(1);
    }
    log_msg("main uruchomil rejestr");

    // uruchamiamy proces generator
    log_msg("main uruchamia generator");
    p_id[++n] = fork();
    if (p_id[n] == 0)
    {
        execl("Procesy/generator_petent", "Procesy/generator_petent", key_str, NULL);
        perror("main - execl generator");
        log_msg("error execl generator");
        cleanup();
        exit(1);
    }
    log_msg("main uruchomil generator");

    // uruchamiamy proces dyrektor
    log_msg("main uruchamia dyrektor");
    p_id[++n] = fork();
    if (p_id[n] == 0)
    {
        execl("Procesy/dyrektor", "Procesy/dyrektor", key_str, NULL);
        perror("main - execl dyrektor");
        log_msg("error execl dyrektor");
        cleanup();
        exit(1);
    }
    log_msg("main uruchomil dyrektor");

    printf("Wszystkie procesy uruchomione\n");
}

void init_sem(int sems)
{
    log_msg("main ustawia semafory");
    union semun arg;
    arg.val = 1;
    if (semctl(sems, SEMAFOR_MAIN, SETVAL, arg) == -1)
    {
        perror("main semctl");
        log_msg("error semctl SETVAL");
        cleanup();
        exit(1);
    }
    arg.val = 0;
    if (semctl(sems, SEMAFOR_DYREKTOR, SETVAL, arg) == -1)
    {
        perror("main semctl");
        log_msg("error semctl SETVAL");
        cleanup();
        exit(1);
    }
    if (semctl(sems, SEMAFOR_GENERATOR, SETVAL, arg) == -1)
    {
        perror("main semctl");
        log_msg("error semctl SETVAL");
        cleanup();
        exit(1);
    }
    if (semctl(sems, SEMAFOR_REJESTR, SETVAL, arg) == -1)
    {
        perror("main semctl");
        log_msg("error semctl SETVAL");
        cleanup();
        exit(1);
    }
    if (semctl(sems, SEMAFOR_START, SETVAL, arg) == -1)
    {
        perror("main semctl");
        log_msg("error semctl SETVAL");
        cleanup();
        exit(1);
    }
    if (semctl(sems, SEMAFOR_PETENCI, SETVAL, arg) == -1)
    {
        perror("main semctl");
        log_msg("error semctl SETVAL");
        cleanup();
        exit(1);
    }
    log_msg("main ustawil semafory");
}

void send_dyrektor(int sems, key_t *shared_mem)
{
    log_msg("main wysyla do dyrektor");
    struct sembuf P = {.sem_num = SEMAFOR_MAIN, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = +1, .sem_flg = 0};
    for (int i = 0; i < ILE_PROCESOW; i++)
    {
        log_msg("main blokuje semafor MAIN");
        while (semop(sems, &P, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("main semop P");
                log_msg("error semop dyrektor");
                cleanup();
                exit(1);
            }
        }

        *shared_mem = p_id[i];

        log_msg("main oddaje semafor DYREKTOR");
        while (semop(sems, &V, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("main semop V");
                cleanup();
                log_msg("error semop dyrektor");
                exit(1);
            }
        }
    }
    log_msg("main wyslal do dyrektor");
}

void log_msg(char *msg)
{
    t = time(NULL);
    t_broken = localtime(&t);
    fprintf(f, "<%02d:%02d:%02d> %s\n", t_broken->tm_hour, t_broken->tm_min, t_broken->tm_sec, msg);
    fflush(f);
}