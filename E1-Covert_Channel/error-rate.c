#include<stdio.h>
#include<string.h>
int dist[10005][10005];
int min(int a, int b, int c)
{
	int temp = a < b ? a : b; 
	return temp < c ? temp : c;
}
 
int myCharDistance(char str1[], char str2[])
{
	int i, j, len1, len2;

	len1 = strlen(str1);
	len2 = strlen(str2);
 
	for(i = 0; i <= len1; i++)
		dist[i][0] = i;
	for(j = 0; j <= len2; j++)
		dist[0][j] = j;
 
	for(i = 1; i <= len1; i++)
		for(j = 1; j <= len2; j++) {
			if(str1[i - 1] == str2[j - 1])   
				dist[i][j] = dist[i - 1][j - 1];
			else {
				int insert = dist[i][j - 1] + 1;  
				int dele = dist[i - 1][j] + 1;     
				int replace = dist[i - 1][j - 1] + 1;   
				dist[i][j] = min(insert, dele, replace);
			}
		}
 
	return dist[len1][len2];
}

static char *progname;
int usage(void)
{
	printf("%s: [cycles]\n", progname);
	return 1;
}

int main(int argc, char *argv[])
{
     char *A;
     int cycles;
     progname = argv[0];
     
     if (sscanf(argv[1], "%d", &cycles) != 1)
		{return usage();}
	FILE *fp_send=fopen("./send_signal","r");
	FILE *fp_receive=fopen("./receive_signal","r");
	A=(char*)malloc(cycles*sizeof(char));
	fscanf(fp_send,"%s",A);
	char *B;
	B=(char*)malloc(cycles*sizeof(char));
	fscanf(fp_receive,"%s",B);
	  
        printf("error rate is:%.2f %\n", (float)(myCharDistance(A, B))/cycles*100);
	  
	free(A);
	free(B);
   
   return 0;
}
