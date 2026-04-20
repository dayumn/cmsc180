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

// Helper to safely print the matrix without freezing the terminal on large inputs
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
        if (res <= 0) { perror("Network recv failed"); exit(1); }
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

// Create random matrix function 
double** createMat(int n){
    double **matrix = (double**)malloc(n * sizeof(double*));
    for (int i = 0; i < n; i++){
        matrix[i] = (double*)malloc(n * sizeof(double));
        for (int j = 0; j < n; j++){
            matrix[i][j] = (double)(rand() % 100 + 1);
        }
    }
    return matrix;
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    int n, p, s;
    
    // Read n, p, and s as user inputs [cite: 20]
    printf("Enter n (size), p (port), and s (0=Master, 1=Slave): ");
    if (scanf("%d %d %d", &n, &p, &s) != 3) {
        printf("Invalid input.\n");
        return 1;
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

    // ==========================================
    // MASTER LOGIC (s == 0)
    // ==========================================
    if (s == 0) {
        printf("Starting Master...\n");
        
        // a. Create non-zero n x n square matrix M [cite: 22]
        double **M = createMat(n);
        
        // d. Take note of the system time time_before [cite: 26]
        clock_gettime(CLOCK_MONOTONIC, &time_before);

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(ports[0]); 
        inet_pton(AF_INET, ips[0], &serv_addr.sin_addr);

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("Connection Failed"); exit(1);
        }

        // Send the entire matrix size (n), then all rows to Slave 0
        send(sock, &n, sizeof(int), 0);
        for (int r = 0; r < n; r++) {
            send_row(sock, M[r], n * sizeof(double));
        }
        
        // --- ADDED PRINT STATEMENT ---
        print_matrix("Master Sent to Slave 0", M, n, n);

        // Wait for the cascading acknowledgment "ack" from the tree
        char ack[4] = {0};
        recv(sock, ack, 3, 0);
        close(sock);

        // f. Take note of the system time time_after [cite: 31]
        clock_gettime(CLOCK_MONOTONIC, &time_after);

        for(int i = 0; i < n; i++) free(M[i]);
        free(M);

    // ==========================================
    // SLAVE LOGIC (s == 1)
    // ==========================================
    } else {
        // Determine my logical rank based on my assigned port
        int rank = -1;
        for (int i = 0; i < t; i++) {
            if (ports[i] == p) rank = i;
        }
        if (rank == -1) { printf("Slave port %d not found in config!\n", p); exit(1); }

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

        // Wait for parent/master to initiate communication by listening [cite: 34]
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in address;
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY; 
        address.sin_port = htons(p);

        bind(server_fd, (struct sockaddr *)&address, sizeof(address));
        listen(server_fd, 3);

        int addrlen = sizeof(address);
        int parent_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);

        // c. When initiated, take note of time_before [cite: 36]
        clock_gettime(CLOCK_MONOTONIC, &time_before);

        // d. Receive the submatrix assigned to it [cite: 37]
        int current_rows;
        recv(parent_socket, &current_rows, sizeof(int), 0);

        // Allocate local memory block based on what was received
        double **local_M = (double**)malloc(current_rows * sizeof(double*));
        for (int r = 0; r < current_rows; r++) {
            local_M[r] = (double*)malloc(n * sizeof(double));
            recv_row(parent_socket, local_M[r], n * sizeof(double));
        }

        // --- ADDED PRINT STATEMENT ---
        char recv_title[64];
        sprintf(recv_title, "Rank %d Received", rank);
        print_matrix(recv_title, local_M, current_rows, n);

        // ----------------------------------------------------
        // ROUTING PHASE: Shifted Binomial Tree Broadcast
        // ----------------------------------------------------
        int gap = (rank == 0) ? 1 : (get_highest_power_of_2(rank) * 2);

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

            // --- ADDED PRINT STATEMENT ---
            char send_title[64];
            sprintf(send_title, "Rank %d Sent to Rank %d", rank, target_rank);
            // Passing local_M + start_row shifts the pointer to the subset of rows we just sent
            print_matrix(send_title, local_M + start_row, rows_to_send, n);

            // Wait for acknowledgment from child
            char ack[4] = {0};
            recv(sock, ack, 3, 0);
            close(sock);

            current_rows = start_row; 
            gap *= 2;
        }

        // f. Take note of time_after [cite: 42]
        clock_gettime(CLOCK_MONOTONIC, &time_after);

        // e. Send acknowledgment "ack" back up the tree [cite: 41]
        send(parent_socket, "ack", 3, 0);
        close(parent_socket);
        close(server_fd);

        for(int i = 0; i < current_rows; i++) free(local_M[i]); // Keep this clean for valgrind
        free(local_M);
    }

    // (4) Obtain elapsed time [cite: 43]
    double time_elapsed = (time_after.tv_sec - time_before.tv_sec)
                        + (time_after.tv_nsec - time_before.tv_nsec) / 1e9;

    // (5) Output time elapsed at each instance's terminal [cite: 44]
    printf("%.6f\n", time_elapsed);

    return 0;
}