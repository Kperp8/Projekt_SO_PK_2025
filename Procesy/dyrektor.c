#include "common.h"

// TODO: urzędnikom czasami się zamykają kolejki w środku programu
// TODO: klony rejestru zamykają się za szybko, gubią petentów, dodać semafor
// TODO: jeśli dyrektor za szybko dostanie SIGINT, nie usuwa struktur systemu V
// TODO: może generator przepisać, aby moć go dowolnie uruchamiać

time_t Tp, Tk;
int N = 27, K = 9;

FILE *f;
time_t t;
struct tm *t_broken;

key_t key;
pid_t p_id[ILE_PROCESOW];

void SIGINT_handle(int sig);
void EMPTY_handle(int sig);

void cleanup();
int recieve_main(int sems, pid_t *shared_mem);
int send_generator(int sems, pid_t *shared_mem);
int send_rejestr(int sems, pid_t *shared_mem);
void log_msg(char *msg);

int main(int argc, char **argv)
{
    signal(SIGINT, SIGINT_handle);
    signal(SIGUSR1, EMPTY_handle);
    signal(SIGUSR2, EMPTY_handle);
    printf("dyrektor\n");

    time_t now, tp, tk, how_long;
    struct tm tm_tp, tm_tk;

    now = time(NULL);
    srand(now);

    int which = rand() % 2;

    tp = now + 10 + rand() % 20;
    tk = now + 30 + rand() % 60;
    how_long = tk - tp;

    localtime_r(&tp, &tm_tp);
    localtime_r(&tk, &tm_tk);

    f = fopen("./Logi/dyrektor", "w");
    if (!f)
    {
        perror("dyrektor fopen");
        cleanup();
        exit(1);
    }

    log_msg("dyrektor uruchomiony");

    char message[128];
    snprintf(
        message,
        sizeof(message),
        "program uruchomi sie o %02d:%02d:%02d, "
        "zakonczy sie o %02d:%02d:%02d, "
        "wysle sygnal=%d",
        tm_tp.tm_hour, tm_tp.tm_min, tm_tp.tm_sec,
        tm_tk.tm_hour, tm_tk.tm_min, tm_tk.tm_sec,
        which + 1);

    log_msg(message);
    printf("%s\n", message);

    key = atoi(argv[1]); // odbieramy klucz

    sleep(tp - now);
    log_msg("dyrektor uruchamia reszte procesow");
    kill(0, SIGCONT);

    int sems = semget(key, ILE_SEMAFOROW, 0); // semafory
    if (sems == -1)
    {
        perror("dyrektor semget");
        log_msg("error semget main");
        cleanup();
        exit(1);
    }

    int shm_id = shmget(key, sizeof(pid_t), 0); // pamiec
    if (shm_id == -1)
    {
        perror("dyrektor shmget");
        log_msg("error shmget main");
        cleanup();
        exit(1);
    }

    pid_t *shared_mem = (pid_t *)shmat(shm_id, NULL, 0); // podlaczamy pamiec
    if (shared_mem == (pid_t *)-1)
    {
        perror("dyrektor shmat");
        log_msg("error shmat main");
        cleanup();
        exit(1);
    }

    if (recieve_main(sems, shared_mem) != 0)
    {
        perror("dyrektor recieve_main");
        log_msg("error recieve_main");
        cleanup();
        exit(1);
    }
    log_msg("dyrektor odebral od main");

    union semun arg;
    arg.val = N;
    if (semctl(sems, SEMAFOR_PETENCI, SETVAL, arg) == -1)
    {
        perror("dyrektor semctl");
        log_msg("error recieve_main");
        cleanup();
        exit(1);
    }
    arg.val = 1;
    if (semctl(sems, SEMAFOR_DYREKTOR, SETVAL, arg) == -1)
    {
        perror("dyrektor semctl");
        log_msg("error recieve_main");
        cleanup();
        exit(1);
    }

    if (send_generator(sems, shared_mem) != 0)
    {
        perror("dyrektor send_generator");
        log_msg("error send_generator");
        cleanup();
        exit(1);
    }
    log_msg("dyrektor wyslal do generator");

    if (send_rejestr(sems, shared_mem) != 0)
    {
        perror("dyrektor send_rejestr");
        log_msg("error send_rejestr");
        cleanup();
        exit(1);
    }
    log_msg("dyrektor wyslal do rejestr");

    printf("przeslano\n");
    log_msg("dyrektor skonczyl przesyl");

    if (shmdt(shared_mem) != 0)
        perror("dyrektor shmdt");

    time_t n = 0;
    while (n++ < how_long)
        sleep(1);

    // sleep(40);

    // for (int i = 0; i < ILE_PROCESOW; i++)
    kill(0, which == 0 ? SIGUSR1 : SIGUSR2);
    for (int i = 0; i < ILE_PROCESOW; i++)
        waitpid(p_id[i], NULL, 0);

    printf("koniec\n");
    log_msg("dyrektor konczy dzialanie");
    cleanup();

    return 0;
}

void cleanup()
{
    log_msg("dyrektor wykonuje cleanup");
    key = ftok(".", 1);
    if (key == -1)
        perror("dyrektor ftok");
    int semid = semget(key, 0, 0);
    if (semid != -1)
        semctl(semid, 0, IPC_RMID);
    else
        perror("dyrektor semget");
    int shmid = shmget(key, sizeof(pid_t), 0);
    if (shmid != -1)
    {
        pid_t *shared_mem = (pid_t *)shmat(shmid, NULL, 0);
        if (shmdt(shared_mem) == -1)
            perror("dyrektor shmdt");
        shmctl(shmid, IPC_RMID, NULL);
    }
    else
        perror("dyrektor shmid");
    for (int i = 0; i < ILE_PROCESOW; i++)
        kill(p_id[i], SIGINT);
    fclose(f);
}

void SIGINT_handle(int sig)
{
    log_msg("dyrektor przechwycil SIGINT"); // asyns-unsafe, używany dużo powoduje ub
    cleanup();
    exit(0);
}

void EMPTY_handle(int sig)
{
}

int recieve_main(int sems, pid_t *shared_mem)
{
    log_msg("dyrektor odbiera od main");
    struct sembuf P = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_MAIN, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < ILE_PROCESOW; i++) // odbieramy p_id[]
    {
        log_msg("dyrektor blokuje semafor DYREKTOR");
        while (semop(sems, &P, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("dyrektor semop P");
                log_msg("error semop main");
                return 1;
            }
        }

        p_id[i] = *shared_mem;

        log_msg("dyrektor oddaje semafor MAIN");
        while (semop(sems, &V, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("dyrektor semop V");
                log_msg("error semop main");
                return 1;
            }
        }
    }
    return 0;
}

int send_generator(int sems, pid_t *shared_mem)
{
    log_msg("dyrektor wysyla do generator");

    struct sembuf P = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_GENERATOR, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < 2; i++)
    {
        log_msg("dyrektor blokuje semafor DYREKTOR");
        while (semop(sems, &P, 1) == -1) // czekamy czy można wysyłać
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("dyrektor semop P");
                log_msg("error semop generator");
                return 1;
            }
        }

        *shared_mem = i == 0 ? N : p_id[6];

        log_msg("dyrektor oddaje semafor GENERATOR");
        while (semop(sems, &V, 1) == -1) // zaznaczamy, że można czytać
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("dyrektor semop V");
                log_msg("error semop generator");
                return 1;
            }
        }
    }
    return 0;
}

int send_rejestr(int sems, pid_t *shared_mem)
{
    log_msg("dyrektor wysyla do rejestr");
    struct sembuf P = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_REJESTR, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < 8; i++)
    {
        log_msg("dyrektor blokuje semafor DYREKTOR");
        while (semop(sems, &P, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("main semop P");
                log_msg("error semop rejestr");
                cleanup();
                exit(1);
            }
        }

        switch (i)
        {
        case 5:
            *shared_mem = K;
            break;
        case 6:
            *shared_mem = N;
            break;
        case 7:
            *shared_mem = p_id[7];
            break;
        default:
            *shared_mem = p_id[i];
        }

        log_msg("dyrektor oddaje semafor REJESTR");
        while (semop(sems, &V, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("main semop V");
                log_msg("error semop rejestr");
                cleanup();
                exit(1);
            }
        }
    }
    return 0;
}

void log_msg(char *msg)
{
    t = time(NULL);
    t_broken = localtime(&t);
    fprintf(f, "<%02d:%02d:%02d> %s\n", t_broken->tm_hour, t_broken->tm_min, t_broken->tm_sec, msg);
    fflush(f);
}