#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>

#define MAX_NUM_CPU 10
#define TIME_DIFF 1 // 1 second

void *memset(void *s, int c, size_t n);

typedef struct cpu {
    char cpuname[100];
    int t_user;   //time spent on processes executing in user mode with normal priority
    int t_nice;   //time spent on processes executing in user mode with “niced” priority
    int t_system; //time spent on processes executing in kernel mode 
    int t_idle;   //time spent idling (i.e. with no CPU instructions)
    int t_iowait; //time spent idling while there were outstanding disk I/O requests
    int t_irq;    //time spent servicing interrupt requests.
    int t_softirq;//time spent servicing softirq
    int t_steal;  //time to run other operating systems in a virtualized environment  
}CPUStruct;

double result[MAX_NUM_CPU];
CPUStruct **cpu_stat, **cpu_stat_prev;
int numCPU; // numbers CPU

pthread_t reader_thread;
pthread_t analyzer_thread;
pthread_t printer_thread;
pthread_mutex_t mutex;
sem_t SemAnalyzer;
sem_t SemReader;
sem_t SemPrinter;

volatile sig_atomic_t done = 0;
 
void term()
{
    done = 1;
}

void skip_lines(FILE *f, int num_lines_skip)
{
    int cnt = 0;
    char c;
    while((cnt < num_lines_skip) && ((c = getc(f)) != EOF))
    {
        if (c == '\n')
            cnt++;
    }
}

void *reader()
{
    while(!done)
    {
        sem_wait(&SemReader);
        pthread_mutex_lock(&mutex);
        FILE *f = fopen("/proc/stat", "r");
        int i;
        for (i = 0; i < numCPU; i++)
        {   
            skip_lines(f,1);
            fscanf(f, "%s %d %d %d %d %d %d %d %d", &(*cpu_stat_prev[i]->cpuname), &cpu_stat_prev[i]->t_user,
                &cpu_stat_prev[i]->t_nice, &cpu_stat_prev[i]->t_system, &cpu_stat_prev[i]->t_idle, 
                &cpu_stat_prev[i]->t_iowait, &cpu_stat_prev[i]->t_irq,
                &cpu_stat_prev[i]->t_softirq, &cpu_stat_prev[i]->t_steal);
        }
        fclose(f);
        sleep(TIME_DIFF);
        f = fopen("/proc/stat", "r");
        for(i = 0; i < numCPU; i++)
        {
            skip_lines(f,1);
            fscanf(f, "%s %d %d %d %d %d %d %d %d", &(*cpu_stat[i]->cpuname), &cpu_stat[i]->t_user, &cpu_stat[i]->t_nice, 
                &cpu_stat[i]->t_system, &cpu_stat[i]->t_idle, &cpu_stat[i]->t_iowait, &cpu_stat[i]->t_irq,
                &cpu_stat[i]->t_softirq, &cpu_stat[i]->t_steal);
        }
        fclose(f);
        pthread_mutex_unlock(&mutex);
        sem_post(&SemAnalyzer);
    }
    printf("Reader thread completed\n");
    pthread_exit(NULL);
}

void *analyzer()
{
    while(!done)
    {
        sem_wait(&SemAnalyzer);
        pthread_mutex_lock(&mutex);
        int i;
        for (i = 0; i < numCPU; i++)
        {
            int PrevIdle = cpu_stat_prev[i]->t_idle + cpu_stat_prev[i]->t_iowait;
            int Idle = cpu_stat[i]->t_idle + cpu_stat[i]->t_iowait;
            
            int PrevNonIdle = cpu_stat_prev[i]->t_user + cpu_stat_prev[i]->t_nice + \
                            + cpu_stat_prev[i]->t_system + cpu_stat_prev[i]->t_irq + \
                            + cpu_stat_prev[i]->t_softirq + cpu_stat_prev[i]->t_steal;
            int NonIdle = cpu_stat[i]->t_user + cpu_stat[i]->t_nice + cpu_stat[i]->t_system + \
                        + cpu_stat[i]->t_irq + cpu_stat[i]->t_softirq + cpu_stat[i]->t_steal;
            
            int PrevTotal = PrevIdle + PrevNonIdle;
            int Total = Idle + NonIdle;

            double totald = (double)Total - (double)PrevTotal;
            double idled = (double)Idle - (double)PrevIdle;
            double CPU_Percentage = (totald - idled)/totald * 100;
            result[i] = CPU_Percentage;
        }
        pthread_mutex_unlock(&mutex);
        sem_post(&SemPrinter);
    }
    printf("Analyzer thread completed.\n");
    pthread_exit(NULL);
}

void *printer()
{
    while(!done)
    {
        sem_wait(&SemPrinter);
        double resultCPU = 0;
        for(int i = 0; i < numCPU; i++)
        {
            resultCPU += result[i];
            printf("Core%d usage = %lf%%\n",i, result[i]);
        }   
        printf("Average CPU usage = %lf%%\n",resultCPU);
        printf("\n");
        sem_post(&SemReader);
    }
    printf("Printer thread completed\n"); 
    pthread_exit(NULL);
}

int main()
{
    cpu_stat = malloc(sizeof(CPUStruct *) * MAX_NUM_CPU);
    cpu_stat_prev = malloc(sizeof(CPUStruct *) * MAX_NUM_CPU);
    for (int i = 0; i < MAX_NUM_CPU; i++)
    {
        cpu_stat[i] = malloc(sizeof(CPUStruct));
        cpu_stat_prev[i] = malloc(sizeof(CPUStruct));
    }
    
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term;
    sigaction(SIGTERM, &action, NULL);

    numCPU = sysconf(_SC_NPROCESSORS_ONLN);
    printf("Number of cores in the processor: %d\n", numCPU);
    int ProcID = getpid(); // for kill PID
    printf("Process ID: %d\n", ProcID);
    printf("\n");

    pthread_mutex_init(&mutex, NULL);
    sem_init(&SemReader,0,1);
    sem_init(&SemAnalyzer,0,0);
    sem_init(&SemPrinter,0,0);

    if (pthread_create(&reader_thread, NULL, &reader, NULL) != 0){
        perror("Failed to create reader thread");
    }

    if (pthread_create(&analyzer_thread, NULL, &analyzer, NULL) != 0){
        perror("Failed to create analyzer thread");
    }
    
    if (pthread_create(&printer_thread, NULL, &printer, NULL) != 0){
        perror("Failed to create printer thread");
    }

    if (pthread_join(reader_thread,NULL) != 0){
        perror("Failed to join reader thread");
    }

    if (pthread_join(analyzer_thread, NULL) != 0){
        perror("Failed to join analyzer thread");
    }

    if (pthread_join(printer_thread, NULL) != 0){
        perror("Failed to join printer thread");
    }

    for(int i = 0; i < MAX_NUM_CPU; i++)
    {
        free(cpu_stat[i]);
        free(cpu_stat_prev[i]);
    }
    free(cpu_stat);
    free(cpu_stat_prev);

    sem_destroy(&SemReader);
    sem_destroy(&SemAnalyzer);
    sem_destroy(&SemPrinter);
    pthread_mutex_destroy(&mutex);

    printf("Program completed \n");
    return 0;
}
