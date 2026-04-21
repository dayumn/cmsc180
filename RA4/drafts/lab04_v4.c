// PENA, Justin Dayne Bryant L.
// CMSC 180 CD-3L
// Version 4: Shifted Binomial Tree Broadcast + Contiguous 1D Array (HPC Optimized)

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// Helper to safely print the flattened matrix
void print_matrix(const char* title, double *matrix, int rows, int cols) {
    if (cols > 16 || rows > 16) {
        printf("--- %s (%dx%d) [Skipped printing numbers, matrix too large] ---\n", title, rows, cols);
        return;
    }
    printf("--- %s (%dx%d) ---\n", title, rows, cols);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%6.0f ", matrix[i * cols + j]); // 1D Index math
        }
        printf("\n");
    }
    printf("-------------------------\n");
}

// Helper to reliably send a massive contiguous block of memory over TCP
void send_chunk(int socket, const double *data, size_t length_in_bytes) {
    size_t bytes_sent = 0;
    const char *ptr = (const char*)data;
    while (bytes_sent < length_in_bytes) {
        ssize_t res = send(socket, ptr + bytes_sent, length_in_bytes - bytes_sent, 0);
        if (res <= 0) { perror("Network send failed"); exit(1); }
        bytes_sent += res;
    }
}

// Helper to reliably receive a massive contiguous block of memory over TCP
void recv_chunk(int socket, double *data, size_t length_in_bytes) {
    size_t bytes_received = 0;
    char *ptr = (char*)data;
    while (bytes_received < length_in_bytes) {
        ssize_t res = recv(socket, ptr + bytes_received, length_in_bytes - bytes_received, 0);
        if (res <= 0) { perror("Network recv failed"); exit(1); }
        bytes_received += res;
    }
}

// Create contiguous 1D random matrix (HPC Standard)
double* createMat(int n) {
    // Cast to large integer to avoid overflow on 16000x16000
    long long total_elements = (long long)n * n;
    double *matrix = (double*)malloc(total_elements * sizeof(double));
    if (!matrix) { perror("Failed to allocate master matrix"); exit(1); }
    
    for (long long i = 0; i < total_elements; i++) {
        matrix[i] = (double)(rand() % 100 + 1);
    }
    return matrix;
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    int n, p, s;
    
    printf("Enter n (size), p (port), and s (0=Master, 1=Slave): ");
    if (scanf("%d %d %d", &n, &p, &s) != 3) {
        printf("Invalid input.\n");
        return 1;
    }

    struct timespec time_before, time_after;

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
        
        // Create contiguous 1D array
        double *M = createMat(n);
        
        clock_gettime(CLOCK_MONOTONIC, &time_before);

        int current_rows = n;
        int gap = t / 2;
        int sent_sockets[32];
        int num_sent = 0;

        while (gap > 0) {
            int target_rank = gap; 
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

            send(sock, &n, sizeof(int), 0);
            send(sock, &rows_to_send, sizeof(int), 0);
            
            // HPC FIX: Send millions of bytes in a single call instead of thousands of small calls
            size_t bytes_to_send = (size_t)rows_to_send * n * sizeof(double);
            send_chunk(sock, M + (start_row * n), bytes_to_send);
            
            sent_sockets[num_sent++] = sock;
            current_rows -= rows_to_send;
            gap /= 2;
        }

        int sock0 = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serv_addr0;
        serv_addr0.sin_family = AF_INET;
        serv_addr0.sin_port = htons(ports[0]); 
        inet_pton(AF_INET, ips[0], &serv_addr0.sin_addr);

        while (connect(sock0, (struct sockaddr *)&serv_addr0, sizeof(serv_addr0)) < 0) { usleep(10000); }

        send(sock0, &n, sizeof(int), 0);
        send(sock0, &current_rows, sizeof(int), 0);
        
        size_t bytes_to_send_0 = (size_t)current_rows * n * sizeof(double);
        send_chunk(sock0, M, bytes_to_send_0);
        
        sent_sockets[num_sent++] = sock0;

        for (int i = 0; i < num_sent; i++) {
            char ack[4] = {0};
            recv(sent_sockets[i], ack, 3, 0);
            close(sent_sockets[i]);
        }

        clock_gettime(CLOCK_MONOTONIC, &time_after);
        free(M);

    // ==========================================
    // SLAVE LOGIC (s == 1)
    // ==========================================
    } else {
        int rank = -1;
        for (int i = 0; i < t; i++) { if (ports[i] == p) rank = i; }
        if (rank == -1) { exit(1); }

        printf("Starting Slave Rank %d...\n", rank);

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

        clock_gettime(CLOCK_MONOTONIC, &time_before);

        int global_n, current_rows;
        recv(parent_socket, &global_n, sizeof(int), 0);
        recv(parent_socket, &current_rows, sizeof(int), 0);

        // HPC FIX: Allocate contiguous block for receiving
        size_t bytes_to_recv = (size_t)current_rows * global_n * sizeof(double);
        double *local_M = (double*)malloc(bytes_to_recv);
        
        recv_chunk(parent_socket, local_M, bytes_to_recv);

        int chunks_held = current_rows / chunk_size;
        int gap = chunks_held / 2;
        int sent_sockets[32]; 
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
            
            size_t bytes_to_send = (size_t)rows_to_send * global_n * sizeof(double);
            send_chunk(sock, local_M + (start_row * global_n), bytes_to_send);

            sent_sockets[num_sent++] = sock;
            current_rows -= rows_to_send; 
            gap /= 2;
        }

        for (int i = 0; i < num_sent; i++) {
            char ack[4] = {0};
            recv(sent_sockets[i], ack, 3, 0);
            close(sent_sockets[i]);
        }

        clock_gettime(CLOCK_MONOTONIC, &time_after);

        send(parent_socket, "ack", 3, 0);
        close(parent_socket);
        close(server_fd);
        free(local_M);
    }

    double time_elapsed = (time_after.tv_sec - time_before.tv_sec)
                        + (time_after.tv_nsec - time_before.tv_nsec) / 1e9;
    printf("%.6f\n", time_elapsed);

    return 0;
}
