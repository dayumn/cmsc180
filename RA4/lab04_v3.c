// PENA, Justin Dayne Bryant L.
// CMSC 180 CD-3L
// Version 3: Fully Parallel Shifted Binomial Tree Broadcast (Including Master)

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// Helper to safely print the matrix
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

    // Read config file
    FILE *cfg = fopen("config_slaves.txt", "r");
    if (!cfg) { perror("Cannot open config_slaves.txt"); exit(1); }
    
    int t; 
    fscanf(cfg, "%d", &t); 
    
    char ips[t][64];
    int ports[t];
    for (int i = 0; i < t; i++) {
        fscanf(cfg, "%s %d", ips[i], &ports[i]);
    }
    fclose(cfg);

    int chunk_size = n / t;

    // ==========================================
    // MASTER LOGIC (s == 0)
    // ==========================================
    if (s == 0) {
        printf("Starting Master...\n");
        
        // a. Create non-zero n x n square matrix M
        double **M = createMat(n);
        
        // d. Take note of the system time time_before
        clock_gettime(CLOCK_MONOTONIC, &time_before);

        int current_rows = n;
        int gap = t / 2;
        
        int sent_sockets[32]; // To hold sockets until acks are received
        int num_sent = 0;

        // Master acts as Root (Rank 0) of the binomial tree
        while (gap > 0) {
            int target_rank = gap; // Master sends to offset 'gap'
            int rows_to_send = gap * chunk_size;
            int start_row = current_rows - rows_to_send;

            int sock = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(ports[target_rank]); 
            inet_pton(AF_INET, ips[target_rank], &serv_addr.sin_addr);

            while (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                usleep(10000); 
            }

            // Send metadata then data
            send(sock, &n, sizeof(int), 0);
            send(sock, &rows_to_send, sizeof(int), 0);
            for (int r = start_row; r < current_rows; r++) {
                send_row(sock, M[r], n * sizeof(double));
            }
            
            char send_title[64];
            sprintf(send_title, "Master Sent to Rank %d", target_rank);
            print_matrix(send_title, M + start_row, rows_to_send, n);

            sent_sockets[num_sent++] = sock;
            current_rows -= rows_to_send;
            gap /= 2;
        }

        // Send the final remaining chunk (Chunk 0) to Slave 0
        int sock0 = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serv_addr0;
        serv_addr0.sin_family = AF_INET;
        serv_addr0.sin_port = htons(ports[0]); 
        inet_pton(AF_INET, ips[0], &serv_addr0.sin_addr);

        while (connect(sock0, (struct sockaddr *)&serv_addr0, sizeof(serv_addr0)) < 0) {
            usleep(10000); 
        }

        send(sock0, &n, sizeof(int), 0);
        send(sock0, &current_rows, sizeof(int), 0);
        for (int r = 0; r < current_rows; r++) {
            send_row(sock0, M[r], n * sizeof(double));
        }

        char send_title[64];
        sprintf(send_title, "Master Sent to Rank 0");
        print_matrix(send_title, M, current_rows, n);
        
        sent_sockets[num_sent++] = sock0;

        // Wait for all Acks non-blockingly for the tree!
        // (Blocking per socket here is fine since they run parallel down the tree)
        for (int i = 0; i < num_sent; i++) {
            char ack[4] = {0};
            recv(sent_sockets[i], ack, 3, 0);
            close(sent_sockets[i]);
        }

        // f. Take note of the system time time_after
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

        // Wait for connection (from Master or Parent slave)
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

        int global_n, current_rows;
        recv(parent_socket, &global_n, sizeof(int), 0);
        recv(parent_socket, &current_rows, sizeof(int), 0);

        // Allocate local memory block
        double **local_M = (double**)malloc(current_rows * sizeof(double*));
        for (int r = 0; r < current_rows; r++) {
            local_M[r] = (double*)malloc(global_n * sizeof(double));
            recv_row(parent_socket, local_M[r], global_n * sizeof(double));
        }

        char recv_title[64];
        sprintf(recv_title, "Rank %d Received", rank);
        print_matrix(recv_title, local_M, current_rows, global_n);

        // ----------------------------------------------------
        // ROUTING PHASE: Shifted Binomial Tree Broadcast
        // ----------------------------------------------------
        int chunks_held = current_rows / chunk_size;
        int gap = chunks_held / 2;

        int sent_sockets[32]; // To hold connection to children
        int num_sent = 0;

        while (gap > 0) {
            int target_rank = rank + gap;
            int rows_to_send = gap * chunk_size; 
            int start_row = current_rows - rows_to_send; 

            int sock = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(ports[target_rank]);
            inet_pton(AF_INET, ips[target_rank], &serv_addr.sin_addr);

            while (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                usleep(10000); 
            }

            send(sock, &global_n, sizeof(int), 0);
            send(sock, &rows_to_send, sizeof(int), 0);
            for (int r = start_row; r < current_rows; r++) {
                send_row(sock, local_M[r], global_n * sizeof(double));
            }

            char send_title[64];
            sprintf(send_title, "Rank %d Sent to Rank %d", rank, target_rank);
            print_matrix(send_title, local_M + start_row, rows_to_send, global_n);

            sent_sockets[num_sent++] = sock;
            current_rows -= rows_to_send; 
            gap /= 2;
        }

        // Wait to receive Acks from all children in parallel
        for (int i = 0; i < num_sent; i++) {
            char ack[4] = {0};
            recv(sent_sockets[i], ack, 3, 0);
            close(sent_sockets[i]);
        }

        // f. Take note of time_after (We consider this node "done" broadcasting)
        clock_gettime(CLOCK_MONOTONIC, &time_after);

        // e. Send acknowledgment "ack" back up the tree to the parent
        send(parent_socket, "ack", 3, 0);
        close(parent_socket);
        close(server_fd);

        for(int i = 0; i < (chunks_held * chunk_size); i++) free(local_M[i]);
        free(local_M);
    }

    // (4) Obtain elapsed time
    double time_elapsed = (time_after.tv_sec - time_before.tv_sec)
                        + (time_after.tv_nsec - time_before.tv_nsec) / 1e9;

    // (5) Output time elapsed at each instance's terminal
    printf("%.6f\n", time_elapsed);

    return 0;
}
