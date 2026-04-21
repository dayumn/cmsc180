// PENA, Justin Dayne Bryant L.
// CMSC 180 CD-3L
// Version 2: Direct Master Broadcast (One-to-Many Personalized)

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
    
    // Read n, p, and s as user inputs
    printf("Enter n (size), p (port), and s (0=Master, 1=Slave): ");
    if (scanf("%d %d %d", &n, &p, &s) != 3) {
        printf("Invalid input.\n");
        return 1;
    }

    struct timespec time_before, time_after;

    // Read config file to determine IPs and ports of the slaves
    FILE *cfg = fopen("config_slaves.txt", "r");
    if (!cfg) { perror("Cannot open config_slaves.txt"); exit(1); }
    
    int t; 
    fscanf(cfg, "%d", &t); // 't' is the number of slaves
    
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
        
        // a. Create non-zero n x n square matrix M
        double **M = createMat(n);
        
        // d. Take note of the system time time_before
        clock_gettime(CLOCK_MONOTONIC, &time_before);

        int rows_per_slave = n / t;

        // e. Distribute the t submatrices to the corresponding slaves directly
        for (int i = 0; i < t; i++) {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(ports[i]); 
            inet_pton(AF_INET, ips[i], &serv_addr.sin_addr);

            // Connect to the specific slave (retry a bit if it's not up right away)
            while (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                usleep(10000); 
            }

            // Send matrix dimensions: full width (n) and the number of rows this slave gets
            send(sock, &n, sizeof(int), 0);
            send(sock, &rows_per_slave, sizeof(int), 0);

            // Send exact subset of rows based on rank
            int start_row = i * rows_per_slave;
            int end_row = start_row + rows_per_slave;
            for (int r = start_row; r < end_row; r++) {
                send_row(sock, M[r], n * sizeof(double));
            }
            
            // f & g. Receive the acknowledgment "ack" from each slave
            char ack[4] = {0};
            recv(sock, ack, 3, 0);
            close(sock);
            
            char send_title[64];
            sprintf(send_title, "Master Sent to Slave %d", i);
            print_matrix(send_title, M + start_row, rows_per_slave, n);
        }

        // h. Take note of the system time time_after
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

        // a. Read from the configuration file what is the IP address of the master
        FILE *cfg_master = fopen("config_master.txt", "r");
        char master_ip[64];
        if (!cfg_master) { perror("Cannot open config_master.txt"); exit(1); }
        fscanf(cfg_master, "%s", master_ip);
        fclose(cfg_master);
        
        printf("Configured to expect data originating from Master IP: %s\n", master_ip);

        // b. Wait for the master to initiate communication by listening
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

        // c. When initiated, take note of time_before
        clock_gettime(CLOCK_MONOTONIC, &time_before);

        // d. Receive the submatrix assigned to it
        int global_n, current_rows;
        recv(parent_socket, &global_n, sizeof(int), 0);
        recv(parent_socket, &current_rows, sizeof(int), 0);

        // Allocate local memory block for the subset
        double **local_M = (double**)malloc(current_rows * sizeof(double*));
        for (int r = 0; r < current_rows; r++) {
            local_M[r] = (double*)malloc(global_n * sizeof(double));
            recv_row(parent_socket, local_M[r], global_n * sizeof(double));
        }

        // e. Send acknowledgment "ack" back to the master
        send(parent_socket, "ack", 3, 0);
        
        // f. Take note of time_after
        clock_gettime(CLOCK_MONOTONIC, &time_after);

        close(parent_socket);
        close(server_fd);

        char recv_title[64];
        sprintf(recv_title, "Rank %d Received", rank);
        print_matrix(recv_title, local_M, current_rows, global_n);

        for(int i = 0; i < current_rows; i++) free(local_M[i]);
        free(local_M);
    }

    // (4) Obtain elapsed time
    double time_elapsed = (time_after.tv_sec - time_before.tv_sec)
                        + (time_after.tv_nsec - time_before.tv_nsec) / 1e9;

    // (5) Output time elapsed at each instance's terminal
    printf("%.6f\n", time_elapsed);

    return 0;
}
