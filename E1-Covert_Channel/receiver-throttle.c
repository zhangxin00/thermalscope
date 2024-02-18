#include <stdio.h>
#define __USE_GNU
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <time.h>
#include <sched.h>
#include <pthread.h>
#include <semaphore.h>
#include <x86intrin.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>


#define	NCPU	256			/* Max CPU's we support		 */
#define	NIRQ	256			/* Max IRQ we support		 */

typedef	unsigned long long	sample_t;
#define	SFMT	"%15llu"		/* Exactly 15 characters wide	 */
#define	TFMT	"%15s"			/* Exactly 15 characters wide	 */
#define	CVT	strtoull		/* Text-to-sample converter	 */

#define ATTACK_CORE 3
#define VICTIM_CORE 3
volatile size_t wait_report=0;
int TRM=0;
static	char *		me = "irqtop";
static	char *		ofile;
static	char *		titles[ NCPU+1 ]; /* CPU{n} we have active	 */
static	char *		irq_names[ NIRQ+1 ]; /* Spelling of IRQ names	 */
static	size_t		ncpu;		/* Number of CPU's we found	 */
static	size_t		nirq;		/* Counts IRQ names we have	 */
static	size_t		nsamples;	/* Elements in sample matrix	 */
static	char *		proc_interrupts = "/proc/interrupts";
static	size_t		debug_level;
int thermal_count[1000006],cycles;
char file[64];
static	void
log_entry(
	int		e,
	char const *	fmt,
	...
)
{
	va_list		ap;

	fprintf( stderr, "%s: ", me );
	va_start( ap, fmt );
	vfprintf(
		stderr,
		fmt,
		ap
	);
	va_end( ap );
	if( e )	{
		fprintf(
			stderr,
			"; %s (errno=%d)",
			strerror( e ),
			e
		);
	}
	fprintf( stderr, ".\n" );
}

static	inline	int
on_debug(
	int		level
)
{
	return(
		(level <= debug_level)
	);
}

static	inline	void
debug(
	int		level,
	char const *	fmt,
	...
)
{
	if( on_debug( level ) )	{
		va_list		ap;

		va_start( ap, fmt );
		vfprintf(
			stderr,
			fmt,
			ap
		);
		va_end( ap );
		fprintf(
			stderr,
			".\n"
		);
	}
}

static	inline	void *
xmalloc(
	size_t	n
)
{
	void *	retval = malloc( n );
	if( !retval )	{
		log_entry(
			errno,
			"cannot allocate %d bytes",
			n
		);
		exit( 1 );
		/* NOTREACHED						 */
	}
	return( retval );
}

static	inline	char *
xstrdup(
	const char *	s
)
{
	char * const	retval = strdup( s );

	if( !retval )	{
		log_entry(
			errno,
			"string allocation failure"
		);
		exit( 1 );
	}
	return( retval );
}

static	inline	sample_t *
new_sample_table(
	void
)
{
	size_t const	space_required = nsamples * sizeof( sample_t );
	sample_t * const	retval = xmalloc( space_required );
	return( retval );
}

static	size_t
discover_irq_setup(
	void
)
{
	size_t		retval;

	retval = -1;
	do	{
		FILE *		f;
		char		buf[ BUFSIZ + 1 ];
		char *		tokens[ 1+((BUFSIZ+1)/2) ];
		char *		token;
		char *		bp;

		f = fopen( proc_interrupts, "rt" );
		if( !f )	{
			log_entry(
				errno,
				"cannot open '%s' for reading.",
				proc_interrupts
			);
			break;
		}
		errno = 0;
		if( !fgets( buf, sizeof( buf ), f ) )	{
			log_entry(
				errno,
				"cannot read '%s'",
				proc_interrupts
			);
			break;
		}
		ncpu = 0;
		for(
			bp = buf;
			(token = strtok( bp, " \t\n" )) != NULL;
			bp = NULL
		)	{
			titles[ ncpu++ ] = xstrdup( token );
			if( ncpu >= NCPU )	{
				break;
			}
		}
		titles[ ncpu ] = NULL;
		/*							 */
		nirq = 0;
		while( fgets( buf, sizeof(buf), f ) )	{
			char * const	irq_name = strtok( buf, " \t\n:" );

			irq_names[ nirq++ ] = xstrdup( irq_name );
			//printf("%s,%d\n",irq_names[nirq-1],nirq-1);
			if(strcmp(irq_names[nirq-1],"TRM")==0) {
				TRM=nirq-1;
			}
			if( nirq >= NIRQ )	{
				break;
			}
			/*
			 * We could skip the ncpu-count tokens and then
			 * take the rest of the line as the interrupt
			 * routing description, but maybe in another update.
			 */
			/* Ignore rest of lien				 */
		}
		irq_names[ nirq ] = NULL;
		if( fclose( f ) )	{
			log_entry(
				errno,
				"cannot close '%s'",
				proc_interrupts
			);
			break;
		}
		nsamples = nirq * ncpu;
		retval = 0;
	} while( 0 );
	return( retval );
}

int get_throttle(){
	int ret; 
	FILE *fp=fopen("/sys/devices/system/cpu/cpu0/thermal_throttle/package_throttle_count","r");
	if(fp==NULL)
   {
   	printf("error!\n");
   	return 0;
   }
	fscanf(fp,"%d",&ret);
	
	//printf("temp:%d\n",ret);
	fclose(fp);
	return ret;

}

static	sample_t *
take_samples(
	void
)
{
	sample_t * const	retval = new_sample_table();
	do	{
		sample_t * const	space = retval;
		FILE *		f;
		char		buf[ BUFSIZ + 1 ];
		sample_t *	sp;

		f = fopen( proc_interrupts, "rt" );
		if( !f )	{
			log_entry(
				errno,
				"cannot open '%s' for samples",
				proc_interrupts
			);
			break;
		}
		/* The first line is the CPU<n> label line		 */
		if( !fgets( buf, BUFSIZ, f ) )	{
			log_entry(
				errno,
				"cannot read CPU titles from '%s'",
				proc_interrupts
			);
			(void) fclose( f );
			break;
		}
		/* All remaining lines are interrupt counts		 */
		sp = space;
		while( fgets( buf, BUFSIZ, f ) )	{
			char *		bp;
			size_t		cpu_no;

			/* Cheap insurance				 */
			buf[ BUFSIZ ] = '\0';
			/* Walk down line past line title's ":"		 */
			for( bp = buf; *bp && (*bp++ != ':'); );
			/* Take next 'ncpu' columns as irq counts	 */
			for( cpu_no = 0; cpu_no < ncpu; ++cpu_no )	{
				char *	eos;

				while( *bp && isspace( *bp ) )	{
					++bp;
				}
				*sp++ = CVT( bp, &bp, 10 );
			}
			/* Ignore the rest, it's just irq routing	 */
		}
		/* Clean up after ourselves				 */
		if( fclose( f ) )	{
			log_entry(
				errno,
				"failed to close sample source '%s'",
				proc_interrupts
			);
			break;
		}
	} while( 0 );
	return( retval );
}


static	sample_t *
sample_diff(
	sample_t const * old,
	sample_t const * new
)
{
	sample_t * const	retval = new_sample_table();

	do	{
		size_t		remain;
		sample_t *	sp;

		for(
			sp = retval,
			remain = nsamples;
			(remain-- > 0);
			*sp++ = *new++ - *old++
		);
	} while( 0 );
	return( retval );
}

int analyze_samples(
	sample_t *	samples
)
{
	time_t const	now = time( NULL );
	size_t		cpu;
	size_t		irq;
	char		tbuf[ 64 ];
	int ret=0;

	//strftime(
		//tbuf,
		//sizeof(tbuf),
		//"%Y%-m-%d %H:%M:%S",
		//localtime( &now )
	//);
	//printf(
	//	"--- %s\n",
	//	tbuf
	//);
	
	//printf( "%3s:", irq_names[TRM] );
	for( irq = 0; irq < nirq; ++irq )	{
	if(irq==TRM){
		//printf( "%3s:", irq_names[ irq ] );
		//for( cpu = 0; cpu < ncpu; ++cpu )	{
		//	printf( "%15llu", *samples++ );
		//}
		//putchar( '\n' );
		for( cpu = 0; cpu < ATTACK_CORE; ++cpu )	{
		*samples++;
		}
		ret=*samples;
		break;
		
		}
	else {
		for( cpu = 0; cpu < ncpu; ++cpu )	{
			*samples++;
		}
	}
	
	}
	return ret;
	
}
int pos=0;
int flag=1;
void sigint(int sig,siginfo_t *siginfo_t,void *context)
{
       flag=0;
       printf("begin!\n");
       return;



}

void sigterm(int sig,siginfo_t *siginfo_t,void *context)
{
     return;



}

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

static void pin_cpu3()
{
	cpu_set_t mask;

	/* PIN to CPU0 */
	CPU_ZERO(&mask);
	CPU_SET(3, &mask);
	sched_setaffinity(0, sizeof(cpu_set_t), &mask);
}

static char *progname;
int usage(void)
{
	printf("%s: [cycles] [out_file]\n", progname);
	return 1;
}


static void pin_cpu(int core)
{
	cpu_set_t mask;

	/* PIN to CPU0 */
	CPU_ZERO(&mask);
	CPU_SET(core, &mask);
	sched_setaffinity(0, sizeof(cpu_set_t), &mask);
}

static void report(int cycles)
{
	pin_cpu(VICTIM_CORE);
	do	{
		static const	struct timespec	period = {
			0, 1e8
		};
		struct itimerspec	interval;
		int		fd;
		sample_t *	old;
		sample_t *	new;
		sample_t *	diff;

		fd = timerfd_create(
			CLOCK_MONOTONIC,
			(0|TFD_CLOEXEC)
		);
		if( fd == -1 )	{
			log_entry(
				errno,
				"cannot create timer"
			);
			break;
		}
		/* Get a baseline sample set				 */
		
		/* Setup the loop interval				 */
		interval.it_interval = period;
		interval.it_value    = period;
		if( timerfd_settime(
			fd,
			0,
			&interval,
			NULL
		) )	{
			log_entry(
				errno,
				"could not establish timer interval"
			);
			break;
		}
		
	struct timeval start,end;
		
	struct timeval start,end;
	gettimeofday(&start,NULL);
	int cnt,pos=0;
	int old_cnt=get_throttle();
	for( int i=0; i<cycles*100 ; i++)	{
	old_cnt=get_throttle();
	__uint64_t	icount;
	ssize_t		nbytes;

	nbytes = read( fd, &icount, sizeof( icount ) );
	if( nbytes != sizeof( icount ) )	{
				log_entry(
					errno,
					"short interval read: %lu",
					nbytes
				);
				break;
	}
	if( icount != 1 )	{
				log_entry(
					0,
					"timer interval overrun: %lu",
					icount
				);
	}
	cnt=get_throttle()-old_cnt;
	old_cnt=cnt;
			
			
	thermal_count[pos]+=cnt;
		
	if(i%100==99){	
	printf("%d\n",thermal_count[pos]);
	if(thermal_count[pos]>100){
			thermal_count[pos]=1;
			}
	else{
			thermal_count[pos]=0;
			}
	pos++;
			
	}        
	}	
			}
		
	 while( 0 );
}



int main(int argc, char ** argv) {
	int retval,ground_truth=0;
	cpu_set_t get;   
    CPU_ZERO(&get);
    pin_cpu3();
   	if(sched_getaffinity(0,sizeof(get),&get)==-1){
   		printf("warning: can not get cpu affinity/n");
    }
    	
    	
    	
    	int i,num,j,count,cycles;
    	progname = argv[0];
    	
  

    	if (argc < 2)
		return usage();


    	if (sscanf(argv[1], "%d", &cycles) != 1)
		return usage();
    	
	

	retval = EXIT_FAILURE;

		
	discover_irq_setup();


	report(cycles);
	FILE *file0 = fopen("receive_signal","w");
       //printf("capture!\n");
       if(file0==NULL){
       printf("error!\n");
         exit(0);
       
       }
       for(int i=0;i<cycles;i++){
       fprintf(file0,"%d",thermal_count[i]);
       
       }
       fclose(file0);
	return 0;
	
	return( retval );
}
