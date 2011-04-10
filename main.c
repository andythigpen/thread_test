#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>
#include <string.h>

#define MAX_ARGS    2
#define NUM_TESTS   1000

struct thread_args
{
    int thread_id;
    int use_csv;
    clockid_t clock_id; 
};

void timespec_subtract( struct timespec *result, 
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

void *thread_test( void *targs )
{
    struct thread_args *args = (struct thread_args *) targs;
    int tid = args->thread_id;
    struct timespec sleep, before, after, diff;
    int i;
    unsigned long min, max, sum = 0;
    sleep.tv_sec = 0;
    sleep.tv_nsec = 10;

    fprintf( stdout, "[%02d] Thread started.\n", tid );
    while ( sleep.tv_nsec < 100000000 ) {
        //fprintf( stdout, "[%02d] Testing sleep %luns.\n", 
        //         tid, sleep.tv_nsec );
        sum = max = 0;
        min = ULONG_MAX;
        for ( i = 0; i < NUM_TESTS; ++i ) {
            clock_gettime( args->clock_id, &before );
            clock_nanosleep( args->clock_id, 0, &sleep, NULL );
            clock_gettime( args->clock_id, &after );
            timespec_subtract( &diff, &after, &before );
            sum += diff.tv_nsec;
            if ( diff.tv_nsec < min ) {
                min = diff.tv_nsec;
            }
            if ( diff.tv_nsec > max ) {
                max = diff.tv_nsec;
            }
            //fprintf( stdout, "[%02d] Difference: %lu.%09lu.\n", 
            //         tid, diff.tv_sec, diff.tv_nsec );
        }
        if ( args->use_csv ) {
            fprintf( stdout, "[%02d] %9lu,%9lu,%9lu,%9lu\n", 
                     tid, sleep.tv_nsec, sum / NUM_TESTS, min, max );
        }
        else {
            fprintf( stdout, "[%02d] Stats %9luns,  "
                     "Avg: %9luns  Min: %9luns  Max: %9luns\n", 
                     tid, sleep.tv_nsec, sum / NUM_TESTS, min, max );
        }
        sleep.tv_nsec *= 10;
    }
    fprintf( stdout, "[%02d] Thread exiting.\n", tid );
    free( targs );
    pthread_exit( NULL );
}

int print_clockres( clockid_t clock_id )
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

void print_usage( const char *basename ) 
{
    fprintf( stderr, "Usage: %s [-f|-r|-o] [-m] [-p priority] [-n threads] [-c]\n", 
             basename );
    fprintf( stderr, "Options:\n" );
    fprintf( stderr, "    -f  use FIFO scheduling\n" );
    fprintf( stderr, "    -r  use ROUND ROBIN scheduling\n" );
    fprintf( stderr, "    -o  use OTHER scheduling\n" );
    fprintf( stderr, "    -m  use MONOTONIC clock\n" );
    fprintf( stderr, "    -n  number of threads to run\n" );
    fprintf( stderr, "    -p  scheduling priority (FIFO or RR)\n" );
    fprintf( stderr, "    -c  print CSV format\n" );
}

int main( int argc, char* argv[] )
{
    pthread_t *threads;
    pthread_attr_t attr;
    struct sched_param param;
    clockid_t clock_id = CLOCK_REALTIME;
    int num_threads = 1;
    int use_sched = 0, policy;
    int use_csv = 0;
    int rc, i, c;
    void *status;

    opterr = 0;
    optind = 1;
    while ( ( c = getopt( argc, argv, "cformp:n:" ) ) != -1 ) {
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
            case 'c':
                use_csv = 1;
                break;
            case 'm':
                clock_id = CLOCK_MONOTONIC;
                fprintf( stdout, "Using MONOTONIC clock.\n" );
                break;
            case 'p':
                if ( !use_sched ) {
                    fprintf( stderr, "Must select a scheduling policy to "
                             "specify a priority.\n" );
                    exit( -1 );
                }
                param.sched_priority = atoi( optarg );
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
