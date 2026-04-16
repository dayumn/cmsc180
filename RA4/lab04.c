// PENA, Justin Dayne Bryant L.
// CMSC 180 CD-3L

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
// New libraries for socket programming
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

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

// create random matrix function given n
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

int main(int argc, char *argv[]) {
    srand(time(NULL));

    int n, p, s;
    
    // 1. (1) Read n, p, and s as user inputs 
    printf("Enter n (size), p (port), and s (0=Master, 1=Slave): ");
    if (scanf("%d %d %d", &n, &p, &s) != 3) {
        printf("Invalid input.\n");
        return 1;
    }

    struct timespec time_before, time_after;

    printf("Starting %s\n", s == 0 ? "Master" : "Slave");

    // ==========================================
    // MASTER LOGIC
    // ==========================================
    if (s == 0) {
        // (2) a. Create non-zero n x n square matrix M [cite: 21, 22]
        double **M = createMat(n);

        // (2) b. Read config to determine IPs, ports, and number of slaves t 
        FILE *cfg = fopen("config_master.txt", "r");
        
        int t;
        fscanf(cfg, "%d", &t); // Read t from the first line of config.txt
        
        char slave_ips[t][64];
        int slave_ports[t];
        for (int i = 0; i < t; i++) {
            fscanf(cfg, "%s %d", slave_ips[i], &slave_ports[i]);
        }
        fclose(cfg);

        // (2) c. Divide M into submatrices of size n/t x n 
        int rows_per_slave = n / t;

        // (2) d. Take note of the system time time_before [cite: 26]
        clock_gettime(CLOCK_MONOTONIC, &time_before);

        // (2) e. Distribute submatrices to corresponding t slaves [cite: 27]
        for (int i = 0; i < t; i++) {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(slave_ports[i]);
            inet_pton(AF_INET, slave_ips[i], &serv_addr.sin_addr);

            // Initiating communication with IP and port of each slave [cite: 28]
            if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                perror("Connection Failed"); exit(1);
            }

            // Send row by row to handle double ** memory mapping
            int start_row = i * rows_per_slave;
            int end_row = start_row + rows_per_slave;
            for (int r = start_row; r < end_row; r++) {
                send_row(sock, M[r], n * sizeof(double));
            }

            // (2) f. Receive the acknowledgment "ack" from each slave 
            char ack[4] = {0};
            recv(sock, ack, 3, 0);
            close(sock);
        }

        // (2) g. Wait when all t slaves have sent respective acknowledgments [cite: 30]
        // (This is implicitly handled by the blocking recv() in the loop above)

        // (2) h. Take note of time time_after [cite: 31]
        clock_gettime(CLOCK_MONOTONIC, &time_after);

        for(int i = 0; i < n; i++) free(M[i]);
        free(M);

    // ==========================================
    // SLAVE LOGIC
    // ==========================================
    } else if (s == 1) {
        // (3) a. Read from config IP of master [cite: 32, 33]
        FILE *cfg = fopen("config_slave.txt", "r");
        char master_ip[64];
        if (cfg) {
            fscanf(cfg, "%s", master_ip); 
            fclose(cfg);
        }

        printf("SLAVE: Waiting for master");

        // (3) b. Wait for master to initiate open port communication by listening [cite: 34]
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in address;
        int opt = 1;
        
        // Prevent bind errors during rapid testing in the lab
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY; 
        address.sin_port = htons(p); // listening to port assigned [cite: 35]

        bind(server_fd, (struct sockaddr *)&address, sizeof(address));
        listen(server_fd, 3);

        int addrlen = sizeof(address);
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);

        // (3) c. When master has initiated, take note of time_before [cite: 36]
        clock_gettime(CLOCK_MONOTONIC, &time_before);

        // Assume t is known or read via config
        int t = 4; 
        
        // Use an environment variable to bypass NFS sync delays
        char *env_t = getenv("SLAVE_T");
        if (env_t) {
            t = atoi(env_t);
        } else {
            FILE *cfg_m = fopen("config_master.txt", "r");
            if (cfg_m) {
                fscanf(cfg_m, "%d", &t);
                fclose(cfg_m);
            } else {
                printf("SLAVE ERROR: Could not open config_master.txt and SLAVE_T not set.\n");
            }
        }
        
        int rows_per_slave = n / t;

        // Allocate local submatrix
        double **submatrix = (double**)malloc(rows_per_slave * sizeof(double*));
        for (int i = 0; i < rows_per_slave; i++) {
            submatrix[i] = (double*)malloc(n * sizeof(double));
        }

        // (3) d. Receive from the master the submatrix m assigned to it 
        for (int r = 0; r < rows_per_slave; r++) {
            recv_row(new_socket, submatrix[r], n * sizeof(double));
        }

        // (3) e. Send an acknowledgment "ack" to the master 
        send(new_socket, "ack", 3, 0);

        // (3) f. Take note of time_after [cite: 42]
        clock_gettime(CLOCK_MONOTONIC, &time_after);

        for(int i = 0; i < rows_per_slave; i++) free(submatrix[i]);
        free(submatrix);
        close(new_socket);
        close(server_fd);
    }

    // (4) Obtain elapsed time [cite: 43]
    double time_elapsed = (time_after.tv_sec - time_before.tv_sec)
                        + (time_after.tv_nsec - time_before.tv_nsec) / 1e9;

    // (5) Output time elapsed at each terminal [cite: 44]
    printf("%.6f\n", time_elapsed);

    return 0;
}
