// PENA, Justin Dayne Bryant L.
// CMSC 180 CD-3L

// COMMENTS: NO NEED FOR STRUCT IN THREADS <- additional overhead
// U DONT HAVE TO TRANSPOSE, JUST KEEP THE COLUMN MAJOR ACCESSING
// 

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#ifdef __linux__
#include <sched.h>
#endif

// thread argument struct
typedef struct {
    double **matrix;
    int m;
    int col_start;
    int col_end;
    int thread_id;
    int cpu_id;
} ThreadArg;

// create random matrix function given n
double** createMat(int n){

    // allocate memory
    double **matrix = (double**)malloc(n*sizeof(double*));

    for (int i = 0; i < n; i++){
        matrix[i] = (double*)malloc(n*sizeof(double));
    }

    // fill in array
    
    for (int i = 0; i < n; i++){
        for (int j = 0; j < n; j++){
            matrix[i][j] =  (double)(rand() % 100 + 1);
        }
    }

    return matrix;
}

// create matrix from input file
double** createMatFromFile(int n, const char* filename){

    double **matrix = (double**)malloc(n*sizeof(double*));

    for (int i = 0; i < n; i++){
        matrix[i] = (double*)malloc(n*sizeof(double));
    }

    FILE *fp = fopen(filename, "r");
    if (!fp){
        printf("Error: could not open %s\n", filename);
        exit(1);
    }

    // skip the first two lines (n and t already read)
    int dummy;
    fscanf(fp, "%d", &dummy);
    fscanf(fp, "%d", &dummy);

    // read comma-separated matrix values
    for (int i = 0; i < n; i++){
        for (int j = 0; j < n; j++){
            if (fscanf(fp, "%lf", &matrix[i][j]) != 1){
                printf("Error reading matrix at [%d][%d]\n", i, j);
                fclose(fp);
                exit(1);
            }
            // consume comma separator if present
            int ch = fgetc(fp);
            if (ch != ',' && ch != EOF){
                ungetc(ch, fp);
            }
        }
    }
    fclose(fp);

    return matrix;
}

// print submatrix for a thread, matrix should be in original (non-transposed) layout
void printSubmatrix(double **matrix, int m, int col_start, int col_end, int thread_id){
    printf("Thread %d submatrix (columns %d to %d):\n", thread_id, col_start, col_end - 1);
    for (int i = 0; i < m; i++){
        for (int j = col_start; j < col_end; j++){
            printf("%8.4f ", matrix[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

// transpose matrix in-place
void transpose(double **matrix, int n){
    for (int i = 0; i < n; i++){
        for (int j = i + 1; j < n; j++){
            double tmp = matrix[i][j];
            matrix[i][j] = matrix[j][i];
            matrix[j][i] = tmp;
        }
    }
}

// mmt function - normalizes columns [col_start, col_end) of the matrix
void mmt(double **matrix, int m, int col_start, int col_end){

    for (int j = col_start; j < col_end; j++){

        // after transpose, column j is now row j → row-major sequential access
        double col_min = matrix[j][0];
        double col_max = matrix[j][0];

        for (int i = 1; i < m; i++){
            if (matrix[j][i] > col_max){
                col_max = matrix[j][i];
            }
            if (matrix[j][i] < col_min){
                col_min = matrix[j][i];
            }
        }

        // normalize (row j in transposed = column j in original)
        for (int i = 0; i < m; i++){
            if (col_max - col_min != 0){
                matrix[j][i] = (matrix[j][i] - col_min)/(col_max - col_min);
            }
        }
    }
}

// threaded mmt wrapper
void* mmtThread(void *arg){

    ThreadArg *targ = (ThreadArg*)arg;

    // call mmt on this thread's column slice (submatrix xi)
    mmt(targ->matrix, targ->m, targ->col_start, targ->col_end);

    return NULL;
}

#ifdef __linux__
// Source - https://stackoverflow.com/a/1407867
// Posted by nos, modified by community. See post 'Timeline' for change history
// Retrieved 2026-03-25, License - CC BY-SA 4.0

int sched_setaffinity(pid_t pid,size_t cpusetsize, cpu_set_t *mask);
#endif


// main function
int main(int argc, char *argv[]){
    srand(time(NULL));

    int n, t;
    int file_mode = (argc > 1);
    double **matrix;

    // ./lab02 input.txt (file mode, reads n from file, t from stdin)
    if (file_mode){
        // read n from the input file, t from stdin
        FILE *fp = fopen(argv[1], "r");
        if (!fp){
            printf("Error: could not open %s\n", argv[1]);
            return 1;
        }
        fscanf(fp, "%d", &n);
        fscanf(fp, "%d", &t);
        fclose(fp);
        matrix = createMatFromFile(n, argv[1]);
    } else {
        scanf("%d", &n);
        scanf("%d", &t);
        matrix = createMat(n);
    }

    // allocate thread arrays
    pthread_t *threads = (pthread_t*)malloc(t * sizeof(pthread_t));
    ThreadArg *args = (ThreadArg*)malloc(t * sizeof(ThreadArg));

    // divide X into t submatrices of size n x n/t each
    int cols_per_thread = n / t;
    int remainder = n % t;

#ifdef __linux__
    // schedule CPU
    cpu_set_t cpuset;
#endif

    // print submatrices before MMT
    if (!file_mode){
        printf("=== Original Matrix ===\n");
        for (int i = 0; i < n; i++){
            for (int j = 0; j < n; j++){
                printf("%8.4f ", matrix[i][j]);
            }
            printf("\n");
        }
        printf("\n=== Submatrices (before MMT) ===\n");
        int offset = 0;
        for (int i = 0; i < t; i++){
            int cols = cols_per_thread + (i < remainder ? 1 : 0);
            printSubmatrix(matrix, n, offset, offset + cols, i);
            offset += cols;
        }
    }

    // time before
    struct timespec time_before, time_after;
    clock_gettime(CLOCK_MONOTONIC, &time_before);

    // // transpose so that columns become rows (cache-friendly access)
    // transpose(matrix, n);

#ifdef __linux__
    // get number of cores (skip core 0)
    int num_cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    int usable_cores = num_cores - 1; // e.g. 3 if 4 cores, 7 if 8 cores
    if (usable_cores < 1) usable_cores = 1;
#endif

    // create t threads, where the ith thread calls mmt(xi, n, n/t)
    int col_offset = 0; // offset adjusted to the last column every i
    for (int i = 0; i < t; i++){
        int cols = cols_per_thread + (i < remainder ? 1 : 0); // gets extra column or not
        args[i].matrix = matrix;
        args[i].m = n;
        args[i].col_start = col_offset;
        args[i].col_end = col_offset + cols;
        args[i].thread_id = i;
        args[i].cpu_id = i; // assigns cpu id to each cpu
        col_offset += cols;

        pthread_create(&threads[i], NULL, mmtThread, &args[i]);

#ifdef __linux__
        // assign thread i to specific core
        CPU_ZERO(&cpuset);
        CPU_SET((i % usable_cores) + 1, &cpuset);
        pthread_setaffinity_np(threads[i], sizeof(cpu_set_t), &cpuset);
#endif
    }

    // join all threads
    for (int i = 0; i < t; i++){
        pthread_join(threads[i], NULL);
    }

    // // transpose back to original layout
    // transpose(matrix, n);

    // time after
    clock_gettime(CLOCK_MONOTONIC, &time_after);

    double time_elapsed = (time_after.tv_sec - time_before.tv_sec)
                        + (time_after.tv_nsec - time_before.tv_nsec) / 1e9;

    // print submatrices after MMT (only in file mode with small n)
    if (!file_mode){
        printf("=== Submatrices (after MMT) ===\n");
        int offset = 0;
        for (int i = 0; i < t; i++){
            int cols = cols_per_thread + (i < remainder ? 1 : 0);
            printSubmatrix(matrix, n, offset, offset + cols, i);
            offset += cols;
        }

        printf("=== Result Matrix T ===\n");
        for (int i = 0; i < n; i++){
            for (int j = 0; j < n; j++){
                printf("%8.4f ", matrix[i][j]);
            }
            printf("\n");
        }
        printf("\n");
    }

    printf("%.6f\n", time_elapsed);

    // free memory
    for (int i = 0; i < n; i++){
        free(matrix[i]);
    }
    free(matrix);
    free(threads);
    free(args);

    return 0;
}