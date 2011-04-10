#include <sys/types.h>
#include <unistd.h>
#include <syscall.h>
/* #include <ctype.h> */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#define MAX_ARGS    2
#define NUM_TESTS   1000

struct thread_args
{
    int thread_id;
    int use_csv;
    int use_abstime;
    int use_timers;
    clockid_t clock_id; 

    /* for signals */
    struct timespec prev;
    unsigned long min;
    unsigned long max;
    unsigned long avg;
    unsigned long sum;
    unsigned long overrun;
};

void
timespec_add( struct timespec *result,
              struct timespec *time1,
              struct timespec *time2 )
{
    /* Add the two times together. */
    result->tv_sec = time1->tv_sec + time2->tv_sec;
    result->tv_nsec = time1->tv_nsec + time2->tv_nsec;
    if ( result->tv_nsec >= 1000000000L ) {        /* Carry? */
        result->tv_sec++;  
        result->tv_nsec = result->tv_nsec - 1000000000L;
    }
}

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

void
sighand( int signo, siginfo_t *siginfo, void *ucntxt )
{
    struct thread_args *args = (struct thread_args *)siginfo->si_ptr;
    struct timespec curr, diff;
    clock_gettime( args->clock_id, &curr );
    timespec_subtract( &diff, &curr, &args->prev );
    args->sum += diff.tv_nsec;
    if ( diff.tv_nsec < args->min ) {
        args->min = diff.tv_nsec;
    }
    if ( diff.tv_nsec > args->max ) {
        args->max = diff.tv_nsec;
    }
    args->prev = curr;
    args->overrun += siginfo->si_overrun;
    //fprintf( stdout, "[%02d] Signal received.\n", args->thread_id );
}

void
timer_test( struct thread_args *args )
{
    struct sigevent evp;
    struct itimerspec its;
    struct sigaction actions;
    timer_t timer_id;

    /* Setup timer event */
    evp.sigev_notify = SIGEV_THREAD_ID;
    evp.sigev_signo = SIGALRM;
    evp.sigev_value.sival_ptr = (void *)args;
    //TODO this may be unsafe...
    // not sure why sigev_notify_thread_id does not work
    evp._sigev_un._tid = syscall(SYS_gettid);
    //evp.sigev_notify_thread_id = gettid();

    if ( timer_create( args->clock_id, &evp, &timer_id ) < 0 ) {
        perror( "timer_create failed" );
        exit( -1 );
    }

    /* Setup signal action */
    memset( &actions, 0, sizeof( actions ) );
    sigemptyset( &actions.sa_mask );
    actions.sa_flags = SA_SIGINFO;
    actions.sa_sigaction = sighand;

    if ( sigaction( SIGALRM, &actions, NULL ) < 0 ) {
        perror( "sigaction failed." );
        exit( -1 );
    }

    if ( !args->use_csv ) {
        fprintf( stdout, "[%02d] |  Stat  |   Avg   |   Min   |   Max   |"
                "  Diff  |  Range  | Overruns |\n", args->thread_id );
    }

    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 1;
    while ( its.it_interval.tv_nsec < 100000000 ) {
        int i;

        args->avg = args->max = args->sum = args->overrun = 0;
        args->min = ULONG_MAX;

        /* turn on timer */
        its.it_value = its.it_interval;
        timer_settime( timer_id, 0, &its, NULL );

        for ( i = 0; i < NUM_TESTS; ++i ) {
            struct timespec remain;
            struct timespec total_sleep = its.it_interval;
            do {
                clock_nanosleep( args->clock_id, 0, &its.it_interval, 
                                 &remain );
                total_sleep = remain;
            } while ( remain.tv_sec > 0 && remain.tv_nsec > 0 );
        }
        if ( args->use_csv ) {
            args->avg = args->sum / NUM_TESTS;
            fprintf( stdout, "[%02d] %lu,%lu,%lu,%lu,%lu,%lu,%lu\n", 
                     args->thread_id, its.it_interval.tv_nsec, 
                     args->avg, args->min, args->max, 
                     args->avg - its.it_interval.tv_nsec, 
                     args->max - args->min, args->overrun );
        }
        else {
            args->avg = args->sum / NUM_TESTS;
            fprintf( stdout, "[%02d] %9lu  %8lu  %8lu  %8lu  %7lu  %8lu  "
                     "%9lu\n", 
                     args->thread_id, its.it_interval.tv_nsec, 
                     args->avg, args->min, args->max, 
                     args->avg - its.it_interval.tv_nsec,
                     args->max - args->min, args->overrun );
        }

        /* turn off timer */
        its.it_value.tv_nsec = 0;
        timer_settime( timer_id, 0, &its, NULL );
        its.it_interval.tv_nsec *= 10;
    }
}

void
sleep_test( struct thread_args *args )
{
    struct timespec sleep, before, after, diff;
    int i;
    unsigned long adjust, avg, min, max, sum = 0;
    sleep.tv_sec = 0;
    sleep.tv_nsec = 1;

    for ( i = 0; i < NUM_TESTS; ++i ) {
        struct timespec temp;
        clock_gettime( args->clock_id, &before );
        clock_gettime( args->clock_id, &temp );
        clock_gettime( args->clock_id, &after );
        timespec_subtract( &diff, &after, &before );
        sum += diff.tv_nsec;
    }
    adjust = ( sum / NUM_TESTS ) << 1; 
    fprintf( stdout, "[%02d] Adjustment for clock_gettime (x2): %9lu.\n", 
             args->thread_id, adjust );

    if ( !args->use_csv ) {
        fprintf( stdout, "[%02d] |  Stat  |   Avg   |   Min   |   Max   |"
                "  Diff  |  Range  |\n", args->thread_id );
    }

    while ( sleep.tv_nsec < 100000000 ) {
        avg = sum = max = 0;
        min = ULONG_MAX;
        for ( i = 0; i < NUM_TESTS; ++i ) {
            if ( !args->use_abstime ) {
                clock_gettime( args->clock_id, &before );
                clock_nanosleep( args->clock_id, 0, &sleep, NULL );
                clock_gettime( args->clock_id, &after );
            }
            else {
                struct timespec wakeup_time;
                clock_gettime( args->clock_id, &before );
                timespec_add( &wakeup_time, &before, &sleep );
                clock_nanosleep( args->clock_id, TIMER_ABSTIME, 
                                 &wakeup_time, NULL );
                clock_gettime( args->clock_id, &after );
            }
            timespec_subtract( &diff, &after, &before );
            sum += diff.tv_nsec - adjust;
            if ( diff.tv_nsec - adjust < min ) {
                min = diff.tv_nsec - adjust;
            }
            if ( diff.tv_nsec - adjust > max ) {
                max = diff.tv_nsec - adjust;
            }
        }
        if ( args->use_csv ) {
            avg = sum / NUM_TESTS;
            fprintf( stdout, "[%02d] %lu,%lu,%lu,%lu,%lu,%lu\n", 
                     args->thread_id, sleep.tv_nsec, 
                     avg, min, max, avg - sleep.tv_nsec, 
                     max - min );
        }
        else {
            avg = sum / NUM_TESTS;
            fprintf( stdout, "[%02d] %9lu  %8lu  %8lu  %8lu  %7lu  %8lu\n", 
                     args->thread_id, sleep.tv_nsec, 
                     avg, min, max, avg - sleep.tv_nsec,
                     max - min );
        }
        sleep.tv_nsec *= 10;
    }

}

void *
thread_test( void *targs )
{
    struct thread_args *args = (struct thread_args *) targs;

    fprintf( stdout, "[%02d] Thread started.\n", args->thread_id );
    if ( args->use_timers ) {
        timer_test( args );
    }
    else {
        sleep_test( args );
    }
    fprintf( stdout, "[%02d] Thread exiting.\n", args->thread_id );
    free( targs );
    pthread_exit( NULL );
}

int 
print_clockres( clockid_t clock_id )
{
    struct timespec res;
    int rc = clock_getres( clock_id, &res );
    if ( rc != 0 ) {
        fprintf( stderr, "clock_getres failed %d.\n", rc );
        return -1;
    }
    fprintf( stdout, "Clock resolution: %luns\n", res.tv_nsec );
    return 0;
}

void 
print_usage( const char *basename ) 
{
    fprintf( stderr, "Usage: %s [-f|-r|-o] [-t] [-m] [-a] [-p priority] [-n threads] [-c]\n", 
             basename );
    fprintf( stderr, "Options:\n" );
    fprintf( stderr, "    -f  use FIFO scheduling\n" );
    fprintf( stderr, "    -r  use ROUND ROBIN scheduling\n" );
    fprintf( stderr, "    -o  use OTHER scheduling\n" );
    fprintf( stderr, "    -t  use timers instead of sleep\n" );
    fprintf( stderr, "    -m  use MONOTONIC clock\n" );
    fprintf( stderr, "    -a  use ABSTIME\n" );
    fprintf( stderr, "    -n  number of threads to run\n" );
    fprintf( stderr, "    -p  scheduling priority (FIFO or RR)\n" );
    fprintf( stderr, "    -c  print CSV format\n" );
}

int 
main( int argc, char* argv[] )
{
    pthread_t *threads;
    pthread_attr_t attr;
    struct sched_param param;
    clockid_t clock_id = CLOCK_REALTIME;
    int num_threads = 1;
    int use_sched = 0, policy;
    int use_csv = 0;
    int use_abstime = 0;
    int use_timers = 0;
    int rc, i, c;
    void *status;

    memset( &param, 0, sizeof( param ) );
    opterr = 0;
    optind = 1;
    while ( ( c = getopt( argc, argv, "cfortmap:n:" ) ) != -1 ) {
        switch ( c )
        {
            case 'f':
            case 'o':
            case 'r':
                if ( use_sched ) {
                    fprintf( stderr, "Only one scheduling policy may "
                             "be selected.\n" );
                    exit( -1 );
                }
                use_sched = 1;
                if ( c == 'f' ) {
                    policy = SCHED_FIFO;
                    fprintf( stdout, "Using FIFO scheduling.\n" );
                }
                else if ( c == 'r' ) {
                    policy = SCHED_RR;
                    fprintf( stdout, "Using ROUND ROBIN scheduling.\n" );
                }
                else if ( c == 'o' ) {
                    policy = SCHED_OTHER;
                    fprintf( stdout, "Using OTHER scheduling.\n" );
                }
                break;
            case 't':
                use_timers = 1;
                break;
            case 'c':
                use_csv = 1;
                break;
            case 'm':
                clock_id = CLOCK_MONOTONIC;
                fprintf( stdout, "Using MONOTONIC clock.\n" );
                break;
            case 'a':
                use_abstime = 1;
                fprintf( stdout, "Using TIMER_ABSTIME.\n" );
                break;
            case 'p':
                param.sched_priority = atoi( optarg );
                fprintf( stdout, "Using priority %d.\n", param.sched_priority );
                break;
            case 'n':
                num_threads = atoi( optarg );
                break;
            case '?':
                if ( optopt == 'p' || optopt == 'n' ) {
                    print_usage( argv[0] );
                    fprintf( stderr, "Missing value for '-%c'.\n", optopt );
                    exit ( -1 );
                }
            default:
                print_usage( argv[0] );
                fprintf( stderr, "Unknown option '-%c'.\n", optopt );
                exit( -1 );
        }
    }

    if ( !use_sched && param.sched_priority ) {
        fprintf( stderr, "Must select a scheduling policy to "
                 "specify a priority.\n" );
        exit( -1 );
    }
    else if ( use_sched && !param.sched_priority ) {
        fprintf( stderr, "Must specify a priority for FIFO or RR.\n" );
        exit( -1 );
    }

    if ( print_clockres( CLOCK_REALTIME ) != 0 ) {
        exit( -1 );
    }

    threads = (pthread_t *) malloc( sizeof(pthread_t) * num_threads );
    if ( threads == NULL ) {
        fprintf( stderr, "pthread_t malloc failed.\n" );
        exit( -1 );
    }

    pthread_attr_init( &attr );
    pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );
    if ( use_sched ) {
        rc = pthread_attr_setinheritsched( &attr, PTHREAD_EXPLICIT_SCHED );
        if ( rc != 0 ) {
            fprintf( stderr, "pthread_attr_setinheritsched failed: %s\n", 
                     strerror( rc ) );
            exit( -1 );
        }
        rc = pthread_attr_setschedpolicy( &attr, policy );
        if ( rc != 0 ) {
            fprintf( stderr, "pthread_attr_setschedpolicy failed: %s\n", 
                     strerror( rc ) );
            exit( -1 );
        }
        rc = pthread_attr_setschedparam( &attr, &param );
        if ( rc != 0 ) {
            fprintf( stderr, "pthread_attr_setschedparam failed: %s\n", 
                     strerror( rc ) );
            exit( -1 );
        }
    }
    
    fprintf( stdout, "Starting %d threads.\n", num_threads );
    for ( i = 0; i < num_threads; ++i ) {
        struct thread_args *args = (struct thread_args *)malloc( 
                sizeof( struct thread_args ) );
        args->thread_id = i;
        args->use_csv = use_csv;
        args->clock_id = clock_id;
        args->use_abstime = use_abstime;
        args->use_timers = use_timers;
        clock_gettime( clock_id, &args->prev );
        rc = pthread_create( &threads[i], &attr, thread_test, args );
        if ( rc ) {
            fprintf( stderr, "[%02d] pthread_create failed: %s.\n", 
                     i, strerror( rc ) );
            exit( -1 );
        }
    }

    for ( i = 0; i < num_threads; ++i ) {
        rc = pthread_join( threads[i], &status );
        if ( rc ) {
            fprintf( stderr, "[%02d] pthread_join failed: %s.\n", 
                     i, strerror( rc ) );
            exit( -1 );
        }
    }
    free( threads );
    fprintf( stdout, "Done.\n" );
    return 0;
}
