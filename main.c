#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>

#define MAX_ARGS    2
#define MAX_THREADS 64
#define NUM_TESTS   1000

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

void *thread_test( void *threadid )
{
    int tid = (int)threadid;
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
            clock_gettime( CLOCK_REALTIME, &before );
            clock_nanosleep( CLOCK_REALTIME, 0, &sleep, NULL );
            clock_gettime( CLOCK_REALTIME, &after );
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
        fprintf( stdout, "[%02d] Stats %9luns,  "
                 "Avg: %9luns  Min: %9luns  Max: %9luns\n", 
                 tid, sleep.tv_nsec, sum / NUM_TESTS, min, max );
        sleep.tv_nsec *= 10;
    }
    fprintf( stdout, "[%02d] Thread exiting.\n", tid );
    pthread_exit( NULL );
}

int main( int argc, char* argv[] )
{
    pthread_t threads[MAX_THREADS];
    pthread_attr_t attr;
    int num_threads;
    int rc, i;
    void *status;

    if ( argc < MAX_ARGS ) {
        fprintf( stderr, "Usage: %s num_threads\n", argv[0] );
        exit( -1 );
    }
    num_threads = atoi( argv[1] );

    pthread_attr_init( &attr );
    pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );
    
    fprintf( stdout, "Starting %d threads.\n", num_threads );
    for ( i = 0; i < num_threads; ++i ) {
        rc = pthread_create( &threads[i], &attr, thread_test, (void *)i );
        if ( rc ) {
            fprintf( stderr, "pthread_create failed %d.\n", rc );
            exit( -1 );
        }
    }

    for ( i = 0; i < num_threads; ++i ) {
        rc = pthread_join( threads[i], &status );
        if ( rc ) {
            fprintf( stderr, "pthread_join failed %d.\n", rc );
            exit( -1 );
        }
    }
    fprintf( stdout, "Done.\n" );
    return 0;
}
