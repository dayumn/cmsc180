// PENA, Justin Dayne Bryant L.
// CMSC 180 CD-3L

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#ifdef __linux__
#include <sched.h>
#endif

// Helper to print the matrix contents
void print_matrix(const char* title, double **matrix, int rows, int cols) {
    if (cols > 16 || rows > 16) {
        printf("--- %s (%dx%d) [Skipped printing numbers, matrix too large] ---\n", title, rows, cols);
        return;
    }
    printf("--- %s (%dx%d) ---\n", title, rows, cols);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%6.0f ", matrix[i][j]);
        }
        printf("\n");
    }
    printf("-------------------------\n");
}

// Helper to reliably send an entire row over TCP
void send_row(int socket, const double *row, size_t length_in_bytes) {
    size_t bytes_sent = 0;
    const char *ptr = (const char*)row;
    while (bytes_sent < length_in_bytes) {
        ssize_t res = send(socket, ptr + bytes_sent, length_in_bytes - bytes_sent, 0);
        if (res <= 0) { perror("Network send failed"); exit(1); }
        bytes_sent += res;
    }
}

// Helper to reliably receive an entire row over TCP
void recv_row(int socket, double *row, size_t length_in_bytes) {
    size_t bytes_received = 0;
    char *ptr = (char*)row;
    while (bytes_received < length_in_bytes) {
        ssize_t res = recv(socket, ptr + bytes_received, length_in_bytes - bytes_received, 0);
        if (res == 0) {
            fprintf(stderr, "Network recv failed: peer closed connection\n");
            exit(1);
        }
        if (res < 0) { perror("Network recv failed"); exit(1); }
        bytes_received += res;
    }
}

// Function to find the highest power of 2 less than or equal to a number
int get_highest_power_of_2(int num) {
    if (num == 0) return 0;
    int p = 1;
    while (p <= num) p *= 2;
    return p / 2;
}

// create matrix from input file
double** createMatFromFile(int n, const char* filename){

    double **matrix = (double**)malloc(n*sizeof(double*));
    if (!matrix) { perror("malloc failed"); exit(1); }

    for (int i = 0; i < n; i++){
        matrix[i] = (double*)malloc(n*sizeof(double));
        if (!matrix[i]) { perror("malloc failed"); exit(1); }
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

// Create random matrix function 
double** createMat(int n){
    double **matrix = (double**)malloc(n * sizeof(double*));
    if (!matrix) { perror("malloc failed"); exit(1); }
    for (int i = 0; i < n; i++){
        matrix[i] = (double*)malloc(n * sizeof(double));
        if (!matrix[i]) { perror("malloc failed"); exit(1); }
        for (int j = 0; j < n; j++){
            matrix[i][j] = (double)(rand() % 100 + 1);
        }
    }
    return matrix;
}

// mmt function - normalizes columns [col_start, col_end) of the matrix
void mmt(double **matrix, int m, int col_start, int col_end){

    for (int j = col_start; j < col_end; j++){

        // after transpose, column j is now row j -> row-major sequential access
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

double **transpose_matrix(double **matrix, int rows, int cols) {
    double **t = (double**)malloc(cols * sizeof(double*));
    for (int j = 0; j < cols; j++) {
        t[j] = (double*)malloc(rows * sizeof(double));
        for (int i = 0; i < rows; i++) {
            t[j][i] = matrix[i][j];
        }
    }
    return t;
}

void free_matrix(double **matrix, int rows) {
    for (int i = 0; i < rows; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    int n, p, s;

    // Read n, p, and s as user inputs [cite: 20]
    if (scanf("%d %d %d", &n, &p, &s) != 3) {
        printf("Invalid input.\n");
        return 1;
    }

    // --- HARDCODED FILE MODE FLAG ---
    // Change to 0 for random matrix, 1 to read from input.txt
    int file_mode = 0;
    int debug = 0;

    if (file_mode) {
        // Read n from the input file to ensure we allocate exactly what input.txt specifies
        FILE *fp = fopen("input.txt", "r");
        if (fp) {
            fscanf(fp, "%d", &n);
            fclose(fp);
        } else {
            printf("Error: could not open input.txt\n");
            return 1;
        }
    }

    struct timespec time_before, time_after;

    // Read config file to determine IPs and ports of the slaves [cite: 23]
    FILE *cfg = fopen("config_slaves.txt", "r");
    if (!cfg) { perror("Cannot open config_slaves.txt"); exit(1); }

    int t;
    fscanf(cfg, "%d", &t); // 't' is the number of slaves [cite: 23]

    char ips[t][64];
    int ports[t];
    for (int i = 0; i < t; i++) {
        fscanf(cfg, "%s %d", ips[i], &ports[i]);
    }
    fclose(cfg);

    int rank = -1;
    if (s == 1) {
        for (int i = 0; i < t; i++) {
            if (ports[i] == p) rank = i;
        }
        if (rank == -1) { printf("Slave port %d not found in config!\n", p); exit(1); }
    }

#ifdef __linux__
    // ---------------------------------------------------------
    // AUTOMATIC CORE AFFINITY
    // ---------------------------------------------------------
    int num_cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    int usable_cores = num_cores - 1;
    if (usable_cores < 1) usable_cores = 1;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    int target_core = 0;

    if (s == 0) {
        // Master process stays pinned to core 0
        target_core = 0;
    } else if (s == 1) {
        // Slave processes use their rank to pick a core (1 to usable_cores)
        target_core = (rank % usable_cores) + 1;
    }

    CPU_SET(target_core, &cpuset);

    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("Warning: sched_setaffinity failed");
    } else {
        printf("Process successfully pinned to core %d\n", target_core);
    }
    // ---------------------------------------------------------
#endif

    // ==========================================
    // MASTER LOGIC (s == 0)
    // ==========================================
    if (s == 0) {
        printf("Starting Master...\n");

        // a. Create n x n square matrix M
        double **M;
        if (file_mode) {
            M = createMatFromFile(n, "input.txt");
        } else {
            M = createMat(n);
        }

        // d. Take note of the system time time_before [cite: 26]
        clock_gettime(CLOCK_MONOTONIC, &time_before);

        // Create a TCP socket
        int sock = socket(AF_INET, SOCK_STREAM, 0);

        // Prepare the server address structure. IPV4, converts ports and string ips into bytes / binary ips
        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(ports[0]);
        inet_pton(AF_INET, ips[0], &serv_addr.sin_addr);

        // Initiate 3 way handshake to slave 0
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("Connection Failed"); exit(1);
        }

        // Send the entire matrix size (n), then all rows to Slave 0
        send(sock, &n, sizeof(int), 0);
        for (int r = 0; r < n; r++) {
            send_row(sock, M[r], n * sizeof(double));
        }

        if (debug) {
            print_matrix("Master Sent to Slave 0", M, n, n);
        }

        // Wait for the gathered matrix from Slave 0
        for (int r = 0; r < n; r++) {
            recv_row(sock, M[r], n * sizeof(double));
        }

        if (debug) {
            print_matrix("Master Gathered from Slave 0", M, n, n);
        }
        close(sock);

        // f. Take note of the system time time_after [cite: 31]
        clock_gettime(CLOCK_MONOTONIC, &time_after);

        free_matrix(M, n);

    // ==========================================
    // SLAVE LOGIC (s == 1)
    // ==========================================
    } else {
        printf("Starting Slave Rank %d...\n", rank);

        // a. Read from the configuration file what is the IP address of the master [cite: 33]
        FILE *cfg_master = fopen("config_master.txt", "r");
        char master_ip[64];
        if (!cfg_master) {
            perror("Cannot open config_master.txt"); exit(1);
        }
        fscanf(cfg_master, "%s", master_ip);
        fclose(cfg_master);

        printf("Configured to expect data originating from Master IP: %s\n", master_ip);

        // Wait for parent/master to initiate communication by listening
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in address;
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // Bind to the port of the current slave
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(p);

        bind(server_fd, (struct sockaddr *)&address, sizeof(address));
        listen(server_fd, 3); // Marks socket as passive

        int addrlen = sizeof(address);
        int parent_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen); // Blocks until parent connects

        // d. Receive the submatrix assigned to it
        int current_rows;
        recv(parent_socket, &current_rows, sizeof(int), 0);

        // Allocate local memory block based on what was received
        double **local_M = (double**)malloc(current_rows * sizeof(double*));
        if (!local_M) { perror("malloc failed"); exit(1); }
        for (int r = 0; r < current_rows; r++) {
            local_M[r] = (double*)malloc(n * sizeof(double));
            if (!local_M[r]) { perror("malloc failed"); exit(1); }
            recv_row(parent_socket, local_M[r], n * sizeof(double));
        }

        if (debug) {
            char recv_title[64];
            sprintf(recv_title, "Rank %d Received", rank);
            print_matrix(recv_title, local_M, current_rows, n);
        }

        // ----------------------------------------------------
        // ROUTING PHASE: Shifted Binomial Tree Broadcast
        // ----------------------------------------------------
        int gap = (rank == 0) ? 1 : (get_highest_power_of_2(rank) * 2);
        int original_rows = current_rows;

        while (rank + gap < t) {
            int target_rank = rank + gap;
            int rows_to_send = current_rows / 2; // Divide submatrices
            int start_row = current_rows - rows_to_send;

            int sock = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(ports[target_rank]);
            inet_pton(AF_INET, ips[target_rank], &serv_addr.sin_addr);

            // Retry connection in case target slave hasn't reached accept() yet
            while (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                usleep(10000);
            }

            // Forward the chunk
            send(sock, &rows_to_send, sizeof(int), 0);
            for (int r = start_row; r < current_rows; r++) {
                send_row(sock, local_M[r], n * sizeof(double));
            }

            if (debug) {
                char send_title[64];
                sprintf(send_title, "Rank %d Sent to Rank %d", rank, target_rank);
                // Passing local_M + start_row shifts the pointer to the subset of rows we just sent
                print_matrix(send_title, local_M + start_row, rows_to_send, n);
            }

            // Wait for processed chunk back from child
            for (int r = start_row; r < current_rows; r++) {
                recv_row(sock, local_M[r], n * sizeof(double));
            }

            if (debug) {
                char gather_title[64];
                sprintf(gather_title, "Rank %d Gathered from Rank %d", rank, target_rank);
                print_matrix(gather_title, local_M + start_row, rows_to_send, n);
            }

            close(sock);

            current_rows = start_row;
            gap *= 2;
        }

        if (debug) {
            char final_title[64];
            sprintf(final_title, "Rank %d Retained", rank);
            print_matrix(final_title, local_M, current_rows, n);
        }

        // f. Take note of time_before and time_after for computation only [cite: 42]
        clock_gettime(CLOCK_MONOTONIC, &time_before);

        // Column-wise major access: transpose retained rows, normalize, then transpose back.
        double **transposed = transpose_matrix(local_M, current_rows, n);
        mmt(transposed, current_rows, 0, n);
        double **normalized = transpose_matrix(transposed, n, current_rows);

        for (int r = 0; r < current_rows; r++) {
            memcpy(local_M[r], normalized[r], n * sizeof(double));
        }

        free_matrix(transposed, n);
        free_matrix(normalized, current_rows);

        clock_gettime(CLOCK_MONOTONIC, &time_after);

        // e. Send gathered chunks back up the tree [cite: 41]
        for (int r = 0; r < original_rows; r++) {
            send_row(parent_socket, local_M[r], n * sizeof(double));
        }

        if (debug) {
            char return_title[64];
            sprintf(return_title, "Rank %d Returned to Parent", rank);
            print_matrix(return_title, local_M, original_rows, n);
        }

        close(parent_socket);
        close(server_fd);

        free_matrix(local_M, original_rows);
    }

    // (4) Obtain elapsed time [cite: 43]
    double time_elapsed = (time_after.tv_sec - time_before.tv_sec)
                        + (time_after.tv_nsec - time_before.tv_nsec) / 1e9;

    // (5) Output time elapsed at each instance's terminal [cite: 44]
    printf("%.6f\n", time_elapsed);

    return 0;
}
