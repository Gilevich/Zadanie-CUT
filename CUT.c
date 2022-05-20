#include <stdio.h>
#include <unistd.h>

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

void reader(CPUStruct *cpu_stat)
{
    FILE *f = fopen("/proc/stat", "r");
    char cpuname[100];
    fscanf(f, "%s %d %d %d %d %d %d %d %d", cpuname, &(cpu_stat->t_user), &(cpu_stat->t_nice), 
        &(cpu_stat->t_system), &(cpu_stat->t_idle), &(cpu_stat->t_iowait), &(cpu_stat->t_irq),
        &(cpu_stat->t_softirq), &(cpu_stat->t_steal));
    fclose(f);
}

double analyzer(CPUStruct *cpu_stat, CPUStruct *cpu_stat_prev)
{
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
    return CPU_Percentage;
}

void printer(double CPU_Percentage)
{
    printf("CPU percentage = %lf%%\n", CPU_Percentage);
}

int main()
{
    CPUStruct cpu_stat, cpu_stat_prev;
    while(1)
    {
        reader(&cpu_stat);
        sleep(1);
        reader(&cpu_stat_prev);
        printer(analyzer(&cpu_stat, &cpu_stat_prev));
    }
    return 0;
}
