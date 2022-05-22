#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define MAX_NUM_CPU 10
#define TIME_DIFF 1 // seconds

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
CPUStruct (*cpu_stat)[MAX_NUM_CPU], (*cpu_stat_prev)[MAX_NUM_CPU];
int numCPU; // numbers CPU

pthread_t reader_thread;
pthread_t analyzer_thread;
pthread_t printer_thread;
pthread_mutex_t mutex;
sem_t SemAnalyzer;
sem_t SemReader;
sem_t SemPrinter;


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
    while(1)
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
}

void *analyzer()
{
    while(1)
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
}

void *printer()
{
    while(1)
    {
        sem_wait(&SemPrinter);
        for(int i = 0; i < numCPU; i++)   
            printf("CPU%d usage = %lf%%\n",i, result[i]);
        printf("\n");
        sem_post(&SemReader);
    } 
    return NULL;
}

int main()
{
    cpu_stat = malloc(sizeof(CPUStruct) * MAX_NUM_CPU);
    cpu_stat_prev = malloc(sizeof(CPUStruct) * MAX_NUM_CPU);
    numCPU = sysconf(_SC_NPROCESSORS_ONLN);
    printf("Number of cores in the processor: %d\n", numCPU);
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
    sem_destroy(&SemReader);
    sem_destroy(&SemAnalyzer);
    sem_destroy(&SemPrinter);
    pthread_mutex_destroy(&mutex);
    return 0;
}
