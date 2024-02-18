#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>
#include <pthread.h>
#include <semaphore.h>
#include <x86intrin.h>
#include <time.h>
#include <math.h>

static unsigned long long int *glob_var,tmp, array[1000000];
void pin_cpu(int n){
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(n, &mask);
    sched_setaffinity(0, sizeof(cpu_set_t),&mask);
}


static char *progname;
int usage(void)
{
	printf("%s: [cycles] [out_file]\n", progname);
	return 1;
}



int main(int argc, char ** argv){
  	int Tnum=10000,T=Tnum, cycles,count;
	char file[64];
	int inter_count[100001]={0};
  	
        if (argc < 3)
		return usage();
	if (sscanf(argv[1], "%d", &cycles) != 1)
		return usage();
	if (sscanf(argv[2], "%s", &file) != 1)
		return usage();
		
        pin_cpu(3);
	for(int i=0;i<1e7;i++){
		i++;
	}
        struct timespec tt1,tt2;
        clock_gettime(CLOCK_BOOTTIME,&tt1);
	unsigned long long last=0;
	last=tt1.tv_nsec;
	for(int i=0;i<cycles;i++){
		count=0;
		clock_gettime(CLOCK_BOOTTIME,&tt2);
		while(tt2.tv_nsec-tt1.tv_nsec<=5*1e6&&tt2.tv_nsec-tt1.tv_nsec>0){
			clock_gettime(CLOCK_BOOTTIME,&tt2);
			count++;
		}
		if(tt2.tv_nsec-tt1.tv_nsec<0){
			tt1=tt2;
			count=0;
			while(tt2.tv_nsec-tt1.tv_nsec<=5*1e6){
			clock_gettime(CLOCK_BOOTTIME,&tt2);
			count++;
		}
		}
		inter_count[i]=count;
		
		tt1=tt2;
		}
	
        
  	FILE *fileo = fopen((char *)file,"w");
       //printf("capture!\n");
       if(fileo==NULL){
       printf("error!\n");
         exit(0);
       
       }
       for(int i=0;i<cycles;i++){
       fprintf(fileo,"%d\n",inter_count[i]);
       
       }
       fclose(fileo);
	return 0;
    }
