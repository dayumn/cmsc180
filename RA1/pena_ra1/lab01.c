#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

double** createMatNonRandom(int *n, const char* filename){

    FILE *fp = fopen(filename, "r");

    // read n from the first line of the file
    fscanf(fp, "%d", n);

    // allocate memory
    double **matrix = (double**)malloc((*n)*sizeof(double*));

    for (int i = 0; i < *n; i++){
        matrix[i] = (double*)malloc((*n)*sizeof(double));
    }

    for (int i = 0; i < *n; i++){
        for (int j = 0; j < *n; j++){
            if (fscanf(fp, "%lf", &matrix[i][j]) != 1){
                printf("Error");
                fclose(fp);
                exit(1);
            }
        }
    }
    fclose(fp);

    return matrix;
}


// mmt function
void mmt(double **matrix, int m, int n){

    // extract min and max
    double col_min[n];
    double col_max[n];

    for (int j = 0; j < n; j++){

        col_min[j] = matrix[0][j];
        col_max[j] = matrix[0][j];

        for (int i = 0; i < m; i++){
            if (matrix[i][j] > col_max[j]){
                col_max[j] = matrix[i][j];
            }
            if (matrix[i][j] < col_min[j]){
                col_min[j] = matrix[i][j];
            }
        }

        for (int i = 0; i < m; i++){
            if (col_max[j] - col_min[j] != 0){
                matrix[i][j] = (matrix[i][j] - col_min[j])/(col_max[j] - col_min[j]);
            } else {
                printf("Division by zero");
            }
        }
    }
    // for (int i = 0; i < n; i++){
    //     printf("%f", col_min[i]);
    //     printf("%f", col_max[i]);
    //     printf("\n");
    // }
    // for (int i = 0; i < n; i++){
    //     for (int j = 0; j < m; j++)
    //         printf("%f ", matrix[i][j]);
    //     printf("\n");
    // }
}

// main function for testing
int main() {
    srand(time(NULL));

    int mode;
    int n;

    printf("Select mode:\n");
    printf("[1] Rxead from input file\n");
    printf("[2] Generate random matrix\n");
    printf("Enter mode: ");
    scanf("%d", &mode);

    double **matrix;

    if (mode == 1){
       matrix = createMatNonRandom(&n, "input.txt");
    }
    else {
        printf("Enter n: ");
        scanf("%d", &n);
        matrix = createMat(n);
    }

    clock_t time_before = clock();

    mmt(matrix, n, n);

    clock_t time_after = clock();

    double time_elapsed = (double)(time_after - time_before) / CLOCKS_PER_SEC;

    printf("%.6f\n", time_elapsed);

    for (int i = 0; i < n; i++){
        free(matrix[i]);
    }
    free(matrix);

    return 0;
}