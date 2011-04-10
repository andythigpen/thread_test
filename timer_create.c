#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>

#define CLOCKID CLOCK_REALTIME
#define SIG SIGRTMIN

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                       } while (0)

void 
timespec_subtract( struct timespec *result, 
                   struct timespec *time1, 
                   struct timespec *time2 )
{
    /* Subtract the second time from the first. */
    if ((time1->tv_sec < time2->tv_sec) ||
        ((time1->tv_sec == time2->tv_sec) &&
        (time1->tv_nsec <= time2->tv_nsec))) {      /* TIME1 <= TIME2? */
        result->tv_sec = result->tv_nsec = 0;
    } else {                        /* TIME1 > TIME2 */
        result->tv_sec = time1->tv_sec - time2->tv_sec;
        if (time1->tv_nsec < time2->tv_nsec) {
            result->tv_nsec = time1->tv_nsec + 1000000000L - time2->tv_nsec;
            result->tv_sec--;               /* Borrow a second. */
        } else {
            result->tv_nsec = time1->tv_nsec - time2->tv_nsec;
        }
    }
}

static void
print_siginfo(siginfo_t *si)
{
   timer_t *tidp;
   int or;

   tidp = si->si_value.sival_ptr;

   printf("    sival_ptr = %p; ", si->si_value.sival_ptr);
   printf("    *sival_ptr = 0x%lx\n", (long) *tidp);

   or = timer_getoverrun(*tidp);
   if (or == -1)
       errExit("timer_getoverrun");
   else
       printf("    overrun count = %d\n", or);
}

static void
handler(int sig, siginfo_t *si, void *uc)
{
    static struct timespec prev;
    struct timespec curr, diff;
   /* Note: calling printf() from a signal handler is not
      strictly correct, since printf() is not async-signal-safe;
      see signal(7) */

    clock_gettime( CLOCK_REALTIME, &curr );
    timespec_subtract( &diff, &curr, &prev );
    printf( "Diff: %lu.%09lu  ", diff.tv_sec, diff.tv_nsec );
   printf("Caught signal %d\n", sig);
   print_siginfo(si);
   prev = curr;
   /* signal(sig, SIG_IGN); */
}

int
main(int argc, char *argv[])
{
   timer_t timerid;
   struct sigevent sev;
   struct itimerspec its;
   long long freq_nanosecs;
   sigset_t mask;
   struct sigaction sa;
   struct timespec sleep, remain;

   if (argc != 3) {
       fprintf(stderr, "Usage: %s <sleep-secs> <freq-nanosecs>\n",
               argv[0]);
       exit(EXIT_FAILURE);
   }

   /* Establish handler for timer signal */

   printf("Establishing handler for signal %d\n", SIG);
   sa.sa_flags = SA_SIGINFO;
   sa.sa_sigaction = handler;
   sigemptyset(&sa.sa_mask);
   if (sigaction(SIG, &sa, NULL) == -1)
       errExit("sigaction");

   /* Block timer signal temporarily */

   /* printf("Blocking signal %d\n", SIG); */
   /* sigemptyset(&mask); */
   /* sigaddset(&mask, SIG); */
   /* if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1) */
   /*     errExit("sigprocmask"); */

   /* Create the timer */

   sev.sigev_notify = SIGEV_SIGNAL;
   sev.sigev_signo = SIG;
   sev.sigev_value.sival_ptr = &timerid;
   if (timer_create(CLOCKID, &sev, &timerid) == -1)
       errExit("timer_create");

   printf("timer ID is 0x%lx\n", (long) timerid);

   /* Start the timer */

   freq_nanosecs = atoll(argv[2]);
   its.it_value.tv_sec = freq_nanosecs / 1000000000;
   its.it_value.tv_nsec = freq_nanosecs % 1000000000;
   its.it_interval.tv_sec = its.it_value.tv_sec;
   its.it_interval.tv_nsec = its.it_value.tv_nsec;

   if (timer_settime(timerid, 0, &its, NULL) == -1)
        errExit("timer_settime");

   /* Sleep for a while; meanwhile, the timer may expire
      multiple times */

   printf("Sleeping for %d seconds\n", atoi(argv[1]));
   /* sleep(atoi(argv[1])); */
   /* sleep(atoi(argv[1])); */
   sleep.tv_sec = atoi(argv[1]);
   sleep.tv_nsec = 0;
   do {
       clock_nanosleep( CLOCK_REALTIME, 0, &sleep, &remain );
       sleep = remain;
   } while ( remain.tv_sec > 0 && remain.tv_nsec > 0 );

   /* Unlock the timer signal, so that timer notification
      can be delivered */

   /* printf("Unblocking signal %d\n", SIG); */
   /* if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) */
   /*     errExit("sigprocmask"); */

   exit(EXIT_SUCCESS);
}


