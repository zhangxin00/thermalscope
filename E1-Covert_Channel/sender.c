#include <stdio.h>
#include <x86intrin.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>

#define M0 10000
#define M1 150000
#define B 0
#define P 16

static char *progname;
int usage(void)
{
   printf("%s: [cycles]\n", progname);
   return 1;
}

int main(int argc, char *argv[])
{

   int cycles;
   progname = argv[0];
   if (argc < 2)
      return usage();

   if (sscanf(argv[1], "%d", &cycles) != 1)
      return usage();
   int str[cycles + 1], secret[cycles];
   memset(str, 0, cycles);
   __m256i a[1024], t;

   int i, j, k, count;
   double time0;
   struct timeval start, end;
   // while( fgets(str, 1024, fp) != NULL ) {

   for (i = 0; i < cycles; i++)
   {
      str[i] = rand() % 2;
   }

   for (i = 0; i < cycles; i++)
   {

      gettimeofday(&start, NULL);
      gettimeofday(&end, NULL);

      if (str[i] == 1)
      {

         if (i > 0 && str[i - 1] == 1)
         {
            while ((end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000 < 3000)
            {
               gettimeofday(&end, NULL);
               for (j = 0; j < M0; j++)
               {
                  sqrt((double)rand());
               }
            }
            sleep(6.0);
         }

         else
         {
            while ((end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000 < 5000)
            {
               gettimeofday(&end, NULL);
               for (j = 0; j < M0; j++)
               {
                  sqrt((double)rand());
               }
            }
            sleep(4.0);
         }
         printf("i=%d,send 1! \n", i);
         while ((end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000 < 10000)
         {
            gettimeofday(&end, NULL);
         }
      }
      else if (str[i] == 0)
      {

         sleep(7.0);
         printf("i=%d,send 0! \n", i);
         while ((end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000 < 10000)
         {
            gettimeofday(&end, NULL);
         }
      }
      /* for (volatile int z = 0; z < 100; z++)
            {
            } /* Delay (can also mfence) */
   }

   FILE *fp_write = fopen("./send_signal", "w");
   for (i = 0; i < cycles; i++)
   {
      // printf("%d",str[i]);
      fprintf(fp_write, "%d", str[i]);
   }
   printf("finish sending\n");

   // memset(str,0,1024);
   //}
   fclose(fp_write);

   return 0;
}
