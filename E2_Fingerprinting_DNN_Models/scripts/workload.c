/*
 *use a whole time slice to count;
 */

#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sched.h>
#include <pthread.h>
#include <semaphore.h>
#include <x86intrin.h>


int inter_count[100001],start[100001],end[100001],cycles;
double score[512]={0};
char file[64];

extern char stopspeculate[];


void set_gs(uint16_t value) {
  __asm__ volatile("mov %0, %%gs" : : "r"(value));
}


uint16_t get_gs() {
  uint16_t value;
  __asm__ volatile("mov %%gs, %0" : "=r"(value));
  return value;
}


int multiply(int num1, int num2)
{
    return num1 * num2;
}

static void pin_cpu2()
{
	cpu_set_t mask;

	/* PIN to CPU0 */
	CPU_ZERO(&mask);
	CPU_SET(2, &mask);
	sched_setaffinity(0, sizeof(cpu_set_t), &mask);
}

static void pin_cpu3()
{
	cpu_set_t mask;

	/* PIN to CPU0 */
	CPU_ZERO(&mask);
	CPU_SET(1, &mask);
	sched_setaffinity(0, sizeof(cpu_set_t), &mask);
}
int count_tick()
{
        int ret,  i,count;
        cpu_set_t get;   
        uint16_t orig_gs = get_gs();
        CPU_ZERO(&get);	
        //pin_cpu3();
    
   	if(sched_getaffinity(0,sizeof(get),&get)==-1)
    	{
   	 printf("warning: can not get cpu affinity/n");
    	}
    	count=0;
     		
     	//get the start of the time slice
	
               
        //use the whole time slice to count
	set_gs(1);
  	
    	while (get_gs() == 1)
    	{  
    	++count;
    	}
    	set_gs(orig_gs);
    
		
    	

	return count;



}
int pos=0;
int flag=1;
void asm_count()
{
	int i;
	unsigned int junk = 0;
	register uint64_t time1, time2;
        unsigned long long tsc0,tsc1;
        while(1)
	{

		unsigned int count=0;
		set_gs(1);
		//start[i]= __rdtscp(&junk)-time1;
		
		
		__asm__ __volatile__(
		//"CLI\n\t"
		"mov $0,%%eax\n\t"
		"L:\n\t" 
		"incl %%eax\n\t"
		"mov %%gs,%%ecx\n\t"
		"cmp $1,%%ecx\n\t" 
		"je L\n\t"
		"mov %%eax, %0" : "=r"(count)
		);
		
		//end[i]=__rdtscp(&junk)-time1;
		//while (get_gs() == 1){count++;}
	//	time2=__rdtscp(&junk);
		//printf("%llu,%llu,%lf\n",time2-time1,count,(double)(time2-time1)/count);
			
		
		}
		
	

		
	
}


static char *progname;

int usage(void)
{
	printf("%s: [cycles] [out_file]\n", progname);
	return 1;
}


void sigint(int sig,siginfo_t *siginfo_t,void *context)
{
       flag=0;
       printf("begin!\n");
       return;



}

void sigterm(int sig,siginfo_t *siginfo_t,void *context)
{
       FILE *fileo = fopen((char *)file,"w");
       //FILE *file2 = fopen("start.txt","w");
      // FILE *file3 = fopen("end.txt","w");
       printf("capture %d!\n",pos);
       if(fileo==NULL){
       printf("error!\n");
         exit(0);
       
       }
       for(int i=0;i<pos;i++){
       fprintf(fileo,"%d\n",inter_count[i]);
       
       }

       fclose(fileo);
     //  fclose(file2);
      // fclose(file3);
       exit(0);



}
//just printf
 
int set_signal(void){
struct sigaction act;
act.sa_sigaction = sigterm;
act.sa_flags=SA_SIGINFO;
return sigaction(SIGTERM,&act,NULL);

}

int set_signal2(void){
struct sigaction act;
act.sa_sigaction = sigint;
act.sa_flags=SA_SIGINFO;
return sigaction(SIGINT,&act,NULL);

}

int main(int argc, char *argv[])
{
	int ret,  i;
        cpu_set_t get;   
        CPU_ZERO(&get);
	progname = argv[0];
	


        pin_cpu3();
   	if(sched_getaffinity(0,sizeof(get),&get)==-1)
    	{
   	 printf("warning: can not get cpu affinity/n");
    	}
    	
	asm_count();
	
	return 0;
	
	
}
