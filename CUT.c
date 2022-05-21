#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct cpu {
    int t_user;   //time spent on processes executing in user mode with normal priority
    int t_nice;   //time spent on processes executing in user mode with “niced” priority
    int t_system; //time spent on processes executing in kernel mode 
    int t_idle;   //time spent idling (i.e. with no CPU instructions)
    int t_iowait; //time spent idling while there were outstanding disk I/O requests
    int t_irq;    //time spent servicing interrupt requests.
    int t_softirq;//time spent servicing softirq
    int t_steal;  //time to run other operating systems in a virtualized environment  
}CPUStruct;

CPUStruct *cpu_stat, *cpu_stat_prev;

void *reader(void *arg)
{
    FILE *f = fopen("/proc/stat", "r");
    char cpuname[100];
    fscanf(f, "%s %d %d %d %d %d %d %d %d", cpuname, &cpu_stat_prev->t_user,
        &cpu_stat_prev->t_nice, &cpu_stat_prev->t_system, &cpu_stat_prev->t_idle, 
        &cpu_stat_prev->t_iowait, &cpu_stat_prev->t_irq,
        &cpu_stat_prev->t_softirq, &cpu_stat_prev->t_steal);
    fclose(f);
    sleep(1);
    f = fopen("/proc/stat", "r");
    fscanf(f, "%s %d %d %d %d %d %d %d %d", cpuname, &cpu_stat->t_user, &cpu_stat->t_nice, 
        &cpu_stat->t_system, &cpu_stat->t_idle, &cpu_stat->t_iowait, &cpu_stat->t_irq,
        &cpu_stat->t_softirq, &cpu_stat->t_steal);
    fclose(f);
    return NULL;
}

void *analyzer(void *arg)
{
    double *result = malloc(sizeof(double));
    int PrevIdle = cpu_stat_prev->t_idle + cpu_stat_prev->t_iowait;
    int Idle = cpu_stat->t_idle + cpu_stat->t_iowait;
    
    int PrevNonIdle = cpu_stat_prev->t_user + cpu_stat_prev->t_nice + cpu_stat_prev->t_system + \
        + cpu_stat_prev->t_irq + cpu_stat_prev->t_softirq + cpu_stat_prev->t_steal;
    int NonIdle = cpu_stat->t_user + cpu_stat->t_nice + cpu_stat->t_system + \
        + cpu_stat->t_irq + cpu_stat->t_softirq + cpu_stat->t_steal;
    

    int PrevTotal = PrevIdle + PrevNonIdle;
    int Total = Idle + NonIdle;


    double totald = (double)Total - (double)PrevTotal;
    double idled = (double)Idle - (double)PrevIdle;
    double CPU_Percentage = (totald - idled)/totald;
    *result = CPU_Percentage;
    return (void *) result;
}

void *printer(void *arg)
{
    double CPU_Percentage = *(double *)arg;
    printf("CPU percentage = %lf%%\n", CPU_Percentage);
    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t reader_thread;
    pthread_t analyzer_thread;
    pthread_t printer_thread;
    cpu_stat = malloc(sizeof(CPUStruct));
    cpu_stat_prev = malloc(sizeof(CPUStruct));
    double *CPU_Percentage_res;
    while(1)
    {
        if (pthread_create(&reader_thread, NULL, &reader, NULL) != 0){
            perror("Failed to create reader thread");
        }

        if (pthread_join(reader_thread,NULL) != 0){
            perror("Failed to join reader thread");
        }

        if (pthread_create(&analyzer_thread, NULL, &analyzer, NULL) != 0){
            perror("Failed to create analyzer thread");
        }

        if (pthread_join(analyzer_thread, (void **)&CPU_Percentage_res) != 0){
            perror("Failed to join analyzer thread");
        }

        if (pthread_create(&printer_thread, NULL, &printer, CPU_Percentage_res) != 0){
            perror("Failed to create printer thread");
        }

        if (pthread_join(printer_thread, NULL) != 0){
            perror("Failed to join printer thread");
        }
        free(CPU_Percentage_res);
    }
    return 0;
}
