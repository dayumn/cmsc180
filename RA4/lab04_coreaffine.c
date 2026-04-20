// PENA, Justin Dayne Bryant L.
// CMSC 180 CD-3L
// LRP04 - Core Affine Implementation

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

// -------------------------------------------------------------------
// NETWORK WRAPPERS
// -------------------------------------------------------------------

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

// -------------------------------------------------------------------
// MATRIX CREATION
// -------------------------------------------------------------------
double** createMat(int n){
    double **matrix = (double**)malloc(n * sizeof(double*));
    for (int i = 0; i < n; i++){
        matrix[i] = (double*)malloc(n * sizeof(double));
    }
    for (int i = 0; i < n; i++){
        for (int j = 0; j < n; j++){
            matrix[i][j] = (double)(rand() % 100 + 1);
        }
    }
    return matrix;
}

// -------------------------------------------------------------------
// MAIN
// -------------------------------------------------------------------
int main(int argc, char *argv[]) {
    srand(time(NULL));

    int n, p, s;
    
    // (1) Read n, p and s as user inputs 
    printf("Enter n (size), p (port), and s (0=Master, 1=Slave): ");
    if (scanf("%d %d %d", &n, &p, &s) != 3) {
        printf("Invalid input.\n");
        return 1;
    }

#ifdef __linux__
    // ---------------------------------------------------------
    // AUTOMATIC CORE AFFINITY 
    // ---------------------------------------------------------
    // Satisfies requirement to run all slave instances in a core-affine way.
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
        // Slave processes use their unique port number to pick a core (1 to usable_cores)
        target_core = (p % usable_cores) + 1;
    }

    CPU_SET(target_core, &cpuset);

    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("Warning: sched_setaffinity failed");
    } else {
        printf("Process successfully pinned to core %d\n", target_core);
    }
    // ---------------------------------------------------------
#endif

    struct timespec time_before, time_after;

    // ==========================================
    // MASTER LOGIC
    // ==========================================
    if (s == 0) {
        // (2) a. Create a non-zero nxn square matrix M whose elements are assigned with random non-zero positive integers[cite: 22].
        double **M = createMat(n);

        // (2) b. Read the configuration file to determine the IP addresses and ports of the slaves and the number of slaves t[cite: 23].
        FILE *cfg = fopen("config_master.txt", "r");
        if (!cfg) { perror("Cannot open config_master.txt"); return 1; }
        
        int t;
        fscanf(cfg, "%d", &t); 
        
        char slave_ips[t][64];
        int slave_ports[t];
        for (int i = 0; i < t; i++) {
            fscanf(cfg, "%s %d", slave_ips[i], &slave_ports[i]);
        }
        fclose(cfg);

        // (2) c. Divide your M into submatrices of size n/t x n each. [cite: 24]
        int rows_per_slave = n / t;

        // (2) d. Take note of the system time time_before[cite: 26].
        clock_gettime(CLOCK_MONOTONIC, &time_before);

        // (2) e. Distribute the submatrices to the corresponding t slaves by opening the port p and initiating communication[cite: 27, 28].
        for (int i = 0; i < t; i++) {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(slave_ports[i]);
            inet_pton(AF_INET, slave_ips[i], &serv_addr.sin_addr);

            if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                perror("Connection Failed"); exit(1);
            }

            // Send row by row 
            int start_row = i * rows_per_slave;
            int end_row = start_row + rows_per_slave;
            for (int r = start_row; r < end_row; r++) {
                send_row(sock, M[r], n * sizeof(double));
            }

            // (2) f. Receive the acknowledgment "ack" from each slave, for all slaves t[cite: 29].
            char ack[4] = {0};
            recv(sock, ack, 3, 0);
            close(sock);
        }

        // (2) f. Take note of the system time time_after[cite: 31].
        // (Implicitly handled by the blocking recv() in the loop above which waits for all acks).
        clock_gettime(CLOCK_MONOTONIC, &time_after);

        for(int i = 0; i < n; i++) free(M[i]);
        free(M);

    // ==========================================
    // SLAVE LOGIC
    // ==========================================
    } else if (s == 1) {
        // (3) a. Read from the configuration file what is the IP address of the master[cite: 33].
        FILE *cfg = fopen("config_slave.txt", "r");
        char master_ip[64];
        if (cfg) {
            fscanf(cfg, "%s", master_ip); 
            fclose(cfg);
        }

        // (3) b. Wait for the master to initiate an open port communication with it by listening to the port assigned[cite: 34, 35].
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
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);

        // (3) c. When the master has initiated, take note of time_before[cite: 36].
        clock_gettime(CLOCK_MONOTONIC, &time_before);

        // Assuming t=2 for general local logic. You can hardcode this depending on the table row you are testing.
        int t = 2; 
        int rows_per_slave = n / t;

        // Allocate local submatrix
        double **submatrix = (double**)malloc(rows_per_slave * sizeof(double*));
        for (int i = 0; i < rows_per_slave; i++) {
            submatrix[i] = (double*)malloc(n * sizeof(double));
        }

        // (3) d. Receive from the master the submatrix m, assigned to it[cite: 37].
        for (int r = 0; r < rows_per_slave; r++) {
            recv_row(new_socket, submatrix[r], n * sizeof(double));
        }

        // (3) e. Send an acknowledgment "ack" to the master once the submatrix have been received fully[cite: 41].
        send(new_socket, "ack", 3, 0);

        // (3) f. Take note of time_after[cite: 42].
        clock_gettime(CLOCK_MONOTONIC, &time_after);

        for(int i = 0; i < rows_per_slave; i++) free(submatrix[i]);
        free(submatrix);
        close(new_socket);
        close(server_fd);
    }

    // (4) Obtain the elapsed time[cite: 43].
    double time_elapsed = (time_after.tv_sec - time_before.tv_sec)
                        + (time_after.tv_nsec - time_before.tv_nsec) / 1e9;

    // (5) Output time elapsed at each instance's terminal[cite: 44].
    printf("%.6f\n", time_elapsed);

    return 0;
}