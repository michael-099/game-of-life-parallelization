/* life_openmp.c  -- corrected version to work with plot.c, real_rand.c, timer.c
   compile with: gcc -O2 -fopenmp life_openmp.c plot.c real_rand.c timer.c -o life_openmp
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <omp.h>

#define MATCH(s) (!strcmp(argv[ac], (s)))

static char **currWorld = NULL, **nextWorld = NULL, **tmesh = NULL;
static int maxiter = 200;
static int population[2] = {0, 0};

int nx = 100;
int ny = 100;

static int w_update = 0;
static int w_plot = 1;

double getTime();
extern FILE *gnu;

/* prototype from plot.c */
int MeshPlot(int t, int m, int n, char **mesh);

/* helper to close gnuplot pipe (plot.c opens it lazily) */
void gnu_close() {
    if (gnu) {
        pclose(gnu);
        gnu = NULL;
    }
}

/* --- Random routines --- */
/* real_rand and seed_rand are provided in real_rand.c; declare externs */
extern int seed_rand(long sd);
extern double real_rand(void);

// double getTime() {
//     struct timeval tv;
//     gettimeofday(&tv, NULL);
//     return tv.tv_sec + 1e-6 * tv.tv_usec;
// }

int main(int argc, char **argv)
{
    int i, j, ac;
    float prob = 0.5;
    long seedVal = 0;
    int game = 0;
    int s_step = 0;
    int numthreads = 1;
    int disable_display = 0;

    /* Parse command line */
    for (ac = 1; ac < argc; ac++)
    {
        if (MATCH("-n")) { nx = atoi(argv[++ac]); }
        else if (MATCH("-i")) { maxiter = atoi(argv[++ac]); }
        else if (MATCH("-t")) { numthreads = atoi(argv[++ac]); if (numthreads < 1) numthreads = 1; }
        else if (MATCH("-p")) { prob = atof(argv[++ac]); }
        else if (MATCH("-s")) { seedVal = atol(argv[++ac]); }
        else if (MATCH("-step")) { s_step = 1; }
        else if (MATCH("-d")) { disable_display = 1; }
        else if (MATCH("-g")) { game = atoi(argv[++ac]); }
        else {
            printf("Usage: %s [-n <meshpoints>] [-i <iterations>] [-s seed] [-p prob] [-t <threads>] [-step] [-g <game #>] [-d]\n", argv[0]);
            return -1;
        }
    }

    int rs = seed_rand(seedVal);
    /* include ghost cells on both dims */
    nx = nx + 2;
    ny = ny + 2;

    /* Allocate contiguous 2D arrays: pointers + data block */
    currWorld = (char **)malloc(sizeof(char *) * nx + sizeof(char) * nx * ny);
    if (!currWorld) { perror("malloc"); return -1; }
    for (i = 0; i < nx; i++) currWorld[i] = (char *)(currWorld + nx) + i * ny;

    nextWorld = (char **)malloc(sizeof(char *) * nx + sizeof(char) * nx * ny);
    if (!nextWorld) { perror("malloc"); free(currWorld); return -1; }
    for (i = 0; i < nx; i++) nextWorld[i] = (char *)(nextWorld + nx) + i * ny;

    /* Zero everything initially (including ghost boundaries) */
    for (i = 0; i < nx; i++)
        for (j = 0; j < ny; j++)
            currWorld[i][j] = nextWorld[i][j] = 0;

    /* Generate initial world */
    population[w_plot] = 0;
    if (game == 0) {
        for (i = 1; i < nx - 1; i++)
            for (j = 1; j < ny - 1; j++) {
                currWorld[i][j] = (real_rand() < prob) ? 1 : 0;
                population[w_plot] += currWorld[i][j];
            }
    }
    else if (game == 1) {
        int nx2 = nx / 2; int ny2 = ny / 2;
        currWorld[nx2 + 1][ny2 + 1] = currWorld[nx2][ny2 + 1] = currWorld[nx2 + 1][ny2] = currWorld[nx2][ny2] = 1;
        population[w_plot] = 4;
    }
    else if (game == 2) {
        int cx = nx / 2, cy = ny / 2;
        currWorld[cx][cy + 1] = 1;
        currWorld[cx + 1][cy + 2] = 1;
        currWorld[cx + 2][cy] = currWorld[cx + 2][cy + 1] = currWorld[cx + 2][cy + 2] = 1;
        population[w_plot] = 5;
    }
    else { printf("Unknown game %d\n", game); exit(-1); }

    printf("probability: %f\nRandom seed: %d\nThreads: %d\n", prob, rs, numthreads);

    /* Initial plot (plot.c will open gnuplot lazily) */
    if (!disable_display) MeshPlot(0, nx - 2, ny - 2, &currWorld[1]);

    omp_set_num_threads(numthreads);
    double t0_total = getTime();
    double t0_comp = getTime();

    int t;
    /* We'll use task parallelism inside a single parallel region */
    #pragma omp parallel
    {
        for (t = 0; t < maxiter && population[w_plot]; t++)
        {
            population[w_update] = 0;
            int interior_rows = nx - 2;   /* rows available for computation (1..nx-2) */

            /* Use single thread to spawn tasks */
            #pragma omp single
            {
                /* Plotter task */
                if (!disable_display && t % 1 == 0) /* plot every iteration */
                    #pragma omp task firstprivate(t) depend(inout: currWorld)
                    MeshPlot(t, nx - 2, ny - 2, &currWorld[1]);

                /* Split rows into tasks (create at most interior_rows tasks; scheduler will map them) */
                int W = omp_get_num_threads();
                int base = interior_rows / W;
                int rem = interior_rows % W;
                int row = 1;
                for (int w = 0; w < W; w++) {
                    int my_rows = base + (w < rem ? 1 : 0);
                    int start = row;
                    int end = start + my_rows - 1;
                    row = end + 1;
                    if (my_rows <= 0) continue;

                    #pragma omp task firstprivate(start, end) shared(currWorld, nextWorld)
                    {
                        for (int ii = start; ii <= end; ii++) {
                            for (int jj = 1; jj < ny - 1; jj++) {
                                int nn = currWorld[ii + 1][jj] + currWorld[ii - 1][jj] +
                                         currWorld[ii][jj + 1] + currWorld[ii][jj - 1] +
                                         currWorld[ii + 1][jj + 1] + currWorld[ii - 1][jj - 1] +
                                         currWorld[ii - 1][jj + 1] + currWorld[ii + 1][jj - 1];
                                nextWorld[ii][jj] = currWorld[ii][jj] ? (nn == 2 || nn == 3) : (nn == 3);
                                if (nextWorld[ii][jj]) {
                                    #pragma omp atomic
                                    population[w_update]++;
                                }
                            }
                        }
                    } /* end task */
                } /* end for workers */

                #pragma omp taskwait

                /* swap worlds */
                tmesh = nextWorld; nextWorld = currWorld; currWorld = tmesh;

                if (s_step) { printf("Step %d\nPress Enter...\n", t); getchar(); }
            } /* end single */

            #pragma omp barrier
        } /* end for t */
    } /* end parallel region */

    double t1_comp = getTime();
    double t1_total = getTime();

    printf("Computation-only time: %f sec\n", t1_comp - t0_comp);
    printf("Total time (including plotting): %f sec\n", t1_total - t0_total);
    printf("Press Enter to exit.\n"); getchar();

    gnu_close();
    free(nextWorld);
    free(currWorld);
    return 0;
}
