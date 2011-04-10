/* Orion Lawlor's Short UNIX Examples, olawlor@acm.org 2004/3/5

Shows how to use setitimer to get periodic interrupts
for pthreads.
*/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#ifdef SOLARIS /* needed with at least Solaris 8 */
#include <siginfo.h>
#endif

void printStack(const char *where) {
	printf("%s stack: %p\n",where,&where);
}

void signalHandler(int cause, siginfo_t *HowCome, void *ptr) {
	printStack("signal handler");
	/* printf("   ptr=%p\n",ptr); // what *is* "ptr"? */
}

void *threadStart(void *ptr)
{
	int i;
	struct itimerval itimer;
    struct timespec sleep, remain;
	
/* Install our SIGPROF signal handler */
        struct sigaction sa;
	printf("thread sigaction: %p\n",&sa);
	
        sa.sa_sigaction = signalHandler;
        sigemptyset( &sa.sa_mask );
        sa.sa_flags = SA_SIGINFO; /* we want a siginfo_t */
        if (sigaction (SIGALRM, &sa, 0)) {
		perror("sigaction");
		exit(1);
        }
	
	printStack("thread routine");

/* Request SIGPROF */
	itimer.it_interval.tv_sec=0;
	itimer.it_interval.tv_usec=10*1000;
	itimer.it_value.tv_sec=0;
	itimer.it_value.tv_usec=10*1000;
	setitimer(ITIMER_REAL, &itimer, NULL); 
	
/* Just wait a bit, which should get a few SIGPROFs */
    sleep.tv_sec = 2;
    sleep.tv_nsec = 0;
    do {
        clock_nanosleep( CLOCK_REALTIME, 0, &sleep, &remain );
        sleep = remain;
    } while ( remain.tv_sec > 0 && remain.tv_nsec > 0 );

	/* for (i=0;i<1000*1000*10;i++) { */
	/* } */
	return 0;
}

int main(){ 
	const int nThreads=5;
	int t;
	pthread_t th;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	printStack("main routine");
	for (t=0;t<nThreads;t++)
		pthread_create(&th,&attr,threadStart,NULL);
	pthread_join(th,NULL);
	
        return(0);
}
/*<@>
<@> ******** Program output: ********
<@> main routine stack: 0x7fffde863260
<@> thread sigaction: 0x40800070
<@> thread routine stack: 0x40800060
<@> thread sigaction: 0x41001070
<@> thread routine stack: 0x41001060
<@> thread sigaction: 0x41802070
<@> thread routine stack: 0x41802060
<@> thread sigaction: 0x42003070
<@> thread routine stack: 0x42003060
<@> thread sigaction: 0x42804070
<@> thread routine stack: 0x42804060
<@> */
