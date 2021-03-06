/*
    Copyright 2005-2012 Intel Corporation.  All Rights Reserved.

    The source code contained or described herein and all documents related
    to the source code ("Material") are owned by Intel Corporation or its
    suppliers or licensors.  Title to the Material remains with Intel
    Corporation or its suppliers and licensors.  The Material is protected
    by worldwide copyright laws and treaty provisions.  No part of the
    Material may be used, copied, reproduced, modified, published, uploaded,
    posted, transmitted, distributed, or disclosed in any way without
    Intel's prior express written permission.

    No license under any patent, copyright, trade secret or other
    intellectual property right is granted to or conferred upon you by
    disclosure or delivery of the Materials, either expressly, by
    implication, inducement, estoppel or otherwise.  Any license under such
    intellectual property rights must be express and approved by Intel in
    writing.
*/

// Declarations for simple estimate of CPU time being used by a program.
// This header is an optional part of the test harness.
// It assumes that "harness_assert.h" has already been included.

#if _WIN32 
#if !_XBOX
    #include <windows.h>
#endif
#else
    #include <sys/time.h>
    #include <sys/resource.h>
#endif

//! Return time (in seconds) spent by the current process in user mode.
/*  Returns 0 if not implemented on platform. */
static double GetCPUUserTime() { 
#if _XBOX
    return 0;
#elif _WIN32
    FILETIME my_times[4];
    bool status = GetProcessTimes(GetCurrentProcess(), my_times, my_times+1, my_times+2, my_times+3)!=0;
    ASSERT( status, NULL );
    LARGE_INTEGER usrtime;
    usrtime.LowPart = my_times[3].dwLowDateTime;
    usrtime.HighPart = my_times[3].dwHighDateTime;
    return double(usrtime.QuadPart)*1E-7;
#else
    // Generic UNIX, including __APPLE__

    // On Linux, there is no good way to get CPU usage info for the current process:
    //   getrusage(RUSAGE_SELF, ...) that is used now only returns info for the calling thread;
    //   getrusage(RUSAGE_CHILDREN, ...) only counts for finished children threads;
    //   tms_utime and tms_cutime got with times(struct tms*) are equivalent to the above items;
    //   finally, /proc/self/task/<task_id>/stat doesn't exist on older kernels 
    //      and it isn't quite convenient to read it for every task_id.

    struct rusage resources;
    bool status = getrusage(RUSAGE_SELF, &resources)==0;
    ASSERT( status, NULL );
    return (double(resources.ru_utime.tv_sec)*1E6 + double(resources.ru_utime.tv_usec))*1E-6;
#endif
}

#include "tbb/tick_count.h"
#include <cstdio>

// The resolution of GetCPUUserTime is 10-15 ms or so; waittime should be a few times bigger.
const double WAITTIME = 0.1; // in seconds, i.e. 100 ms
const double THRESHOLD = WAITTIME/100;

static void TestCPUUserTime( int nthreads, int nactive = 1 ) {
    // The test will always pass on Linux; read the comments in GetCPUUserTime for details
    // Also it will not detect spinning issues on systems with only one processing core.

    int nworkers = nthreads-nactive;
    if( !nworkers ) return;
    double lastusrtime = GetCPUUserTime();
    if( !lastusrtime ) return;

    static double minimal_waittime = WAITTIME,
                  maximal_waittime = WAITTIME * 10;
    double usrtime;
    double waittime;
    tbb::tick_count stamp = tbb::tick_count::now();
    // wait for GetCPUUserTime update
    while( (usrtime=GetCPUUserTime())-lastusrtime < THRESHOLD ) {
        volatile intptr_t k = (intptr_t)&usrtime;
        for ( int i = 0; i < 1000; ++i ) ++k;
        if ( (waittime = (tbb::tick_count::now()-stamp).seconds()) > maximal_waittime ) {
            REPORT( "Warning: %.2f sec elapsed but user mode time is still below its threshold (%g < %g)\n", 
                    waittime, usrtime - lastusrtime, THRESHOLD );
            break;
        }
    }
    lastusrtime = usrtime;
    
    // Wait for workers to go sleep
    stamp = tbb::tick_count::now();
    while( ((waittime=(tbb::tick_count::now()-stamp).seconds()) < minimal_waittime) 
            || ((usrtime=GetCPUUserTime()-lastusrtime) < THRESHOLD) )
    {
        if ( waittime > maximal_waittime ) {
            REPORT( "Warning: %.2f sec elapsed but GetCPUUserTime reported only %g sec\n", waittime, usrtime );
            break;
        }
    }

    // Test that all workers sleep when no work.
    while( nactive>1 && usrtime-nactive*waittime<0 ) {
        // probably the number of active threads was mispredicted
        --nactive; ++nworkers;
    }
    double avg_worker_usrtime = (usrtime-nactive*waittime)/nworkers;

    if( avg_worker_usrtime > waittime/2 )
        REPORT( "ERROR: %d worker threads are spinning; waittime: %g; usrtime: %g; avg worker usrtime: %g\n",
                nworkers, waittime, usrtime, avg_worker_usrtime);
    else
        REMARK("%d worker threads; waittime: %g; usrtime: %g; avg worker usrtime: %g\n",
                        nworkers, waittime, usrtime, avg_worker_usrtime);
}
