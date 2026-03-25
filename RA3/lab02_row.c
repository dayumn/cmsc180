#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

// thread argument struct for row partitioning
typedef struct {
    double **matrix;
    int n;
    int row_start;
    int row_end;
    double *local_min;   // per-column local min found by this thread
    double *local_max;   // per-column local max found by this thread
    double *global_min;  // per-column global min (shared, read-only in phase 2)
    double *global_max;  // per-column global max (shared, read-only in phase 2)
} ThreadArg;

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

// create matrix function given n
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

// phase 1: each thread finds local min and max for its chunk of rows
void* mmtPhase1(void *arg){

    ThreadArg *targ = (ThreadArg*)arg;
    double **matrix = targ->matrix;
    int n = targ->n;
    int row_start = targ->row_start;
    int row_end = targ->row_end;
    double *local_min = targ->local_min;
    double *local_max = targ->local_max;

    // initialize local min/max from first row in this thread's chunk
    for (int j = 0; j < n; j++){
        local_min[j] = matrix[row_start][j];
        local_max[j] = matrix[row_start][j];
    }

    // scan remaining rows to update local min/max
    for (int i = row_start + 1; i < row_end; i++){
        for (int j = 0; j < n; j++){
            if (matrix[i][j] > local_max[j]){
                local_max[j] = matrix[i][j];
            }
            if (matrix[i][j] < local_min[j]){
                local_min[j] = matrix[i][j];
            }
        }
    }

    return NULL;
}

// phase 2: each thread normalizes its chunk of rows using global min/max
void* mmtPhase2(void *arg){

    ThreadArg *targ = (ThreadArg*)arg;
    double **matrix = targ->matrix;
    int n = targ->n;
    int row_start = targ->row_start;
    int row_end = targ->row_end;
    double *global_min = targ->global_min;
    double *global_max = targ->global_max;

    // normalize each element using global min/max per column
    for (int i = row_start; i < row_end; i++){
        for (int j = 0; j < n; j++){
            if (global_max[j] - global_min[j] != 0){
                matrix[i][j] = (matrix[i][j] - global_min[j])/(global_max[j] - global_min[j]);
            }
        }
    }

    return NULL;
}

// main function
int main(int argc, char *argv[]){
    srand(time(NULL));


    int n, t;
    scanf("%d", &n);
    scanf("%d", &t);
    int file_mode = (argc > 1);

    // create random n x n matrix
    double **matrix = createMat(n);

    // allocate thread arrays
    pthread_t *threads = (pthread_t*)malloc(t * sizeof(pthread_t));
    ThreadArg *args = (ThreadArg*)malloc(t * sizeof(ThreadArg));

    // allocate per-thread local min/max arrays
    double **all_local_min = (double**)malloc(t * sizeof(double*));
    double **all_local_max = (double**)malloc(t * sizeof(double*));
    for (int i = 0; i < t; i++){
        all_local_min[i] = (double*)malloc(n * sizeof(double));
        all_local_max[i] = (double*)malloc(n * sizeof(double));
    }

    // allocate global min/max arrays
    double *global_min = (double*)malloc(n * sizeof(double));
    double *global_max = (double*)malloc(n * sizeof(double));

    // partition rows across t threads (n/t rows each)
    int rows_per_thread = n / t;
    int remainder = n % t;

    // set up thread arguments with row ranges
    int row_offset = 0;
    for (int i = 0; i < t; i++){
        int rows = rows_per_thread + (i < remainder ? 1 : 0);
        args[i].matrix = matrix;
        args[i].n = n;
        args[i].row_start = row_offset;
        args[i].row_end = row_offset + rows;
        args[i].local_min = all_local_min[i];
        args[i].local_max = all_local_max[i];
        args[i].global_min = global_min;
        args[i].global_max = global_max;
        row_offset += rows;
    }

    // time before
    struct timespec time_before, time_after;
    clock_gettime(CLOCK_MONOTONIC, &time_before);

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
            int cols = rows_per_thread + (i < remainder ? 1 : 0);
            printSubmatrix(matrix, n, offset, offset + cols, i);
            offset += cols;
        }
    }

    // === PHASE 1: find local min/max per thread ===
    for (int i = 0; i < t; i++){
        pthread_create(&threads[i], NULL, mmtPhase1, &args[i]);
    }
    for (int i = 0; i < t; i++){
        pthread_join(threads[i], NULL);
    }

    // === REDUCE: merge local min/max into global min/max ===
    for (int j = 0; j < n; j++){
        global_min[j] = all_local_min[0][j];
        global_max[j] = all_local_max[0][j];
    }
    for (int i = 1; i < t; i++){
        for (int j = 0; j < n; j++){
            if (all_local_min[i][j] < global_min[j]){
                global_min[j] = all_local_min[i][j];
            }
            if (all_local_max[i][j] > global_max[j]){
                global_max[j] = all_local_max[i][j];
            }
        }
    }

    // === PHASE 2: normalize using global min/max ===
    for (int i = 0; i < t; i++){
        pthread_create(&threads[i], NULL, mmtPhase2, &args[i]);
    }
    for (int i = 0; i < t; i++){
        pthread_join(threads[i], NULL);
    }

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
            int cols = rows_per_thread + (i < remainder ? 1 : 0);
            printSubmatrix(matrix, n, offset, offset + cols, i);
            offset += cols;
        }
    }

    // time after
    clock_gettime(CLOCK_MONOTONIC, &time_after);

    double time_elapsed = (time_after.tv_sec - time_before.tv_sec)
                        + (time_after.tv_nsec - time_before.tv_nsec) / 1e9;

    printf("%.6f\n", time_elapsed);

    // free memory
    for (int i = 0; i < t; i++){
        free(all_local_min[i]);
        free(all_local_max[i]);
    }
    free(all_local_min);
    free(all_local_max);
    free(global_min);
    free(global_max);
    free(matrix[0]);
    free(matrix);
    free(threads);
    free(args);

    return 0;
}
