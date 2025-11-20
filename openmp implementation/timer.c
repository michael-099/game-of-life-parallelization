#include <sys/time.h>
#include <stdio.h>

const double kMicro = 1.0e-6;

double getTime()
{
    struct timeval TV;
    
    // Use NULL for the obsolete timezone argument
    const int RC = gettimeofday(&TV, NULL); 

    if(RC == -1)
    {
        printf("ERROR: Bad call to gettimeofday\n");
        // Return a double error code (-1.0) for consistency
        return -1.0; 
    }
    
    // Calculates the time in seconds (seconds + microseconds * 1e-6)
    return ( (double)TV.tv_sec + kMicro * (double)TV.tv_usec );
} // end getTime()