#ifndef DOCKER_MONITOR_H
#define DOCKER_MONITOR_H

#include <stdint.h>
#include <time.h>

#define MAX_CONTAINERS 100
#define MAX_CONTAINER_NAME 256
#define MAX_JSON_SIZE 8192
#define DOCKER_SOCKET "/var/run/docker.sock"

typedef struct {
    char id[64];
    char name[MAX_CONTAINER_NAME];
    char image[MAX_CONTAINER_NAME];
    char status[32];
    time_t created;
    time_t last_seen;
} container_info_t;

typedef struct {
    uint64_t cpu_usage;
    uint64_t cpu_system_usage;
    uint64_t memory_usage;
    uint64_t memory_limit;
    uint64_t network_rx_bytes;
    uint64_t network_tx_bytes;
    uint64_t block_read_bytes;
    uint64_t block_write_bytes;
    time_t timestamp;
} container_stats_t;

typedef struct {
    container_info_t info;
    container_stats_t stats;
    double cpu_percent;
    double memory_percent;
    int is_running;
} container_monitor_t;

typedef struct {
    char host[256];
    int port;
    int use_tls;
    char cert_path[256];
    char key_path[256];
    char ca_path[256];
} docker_config_t;

typedef struct {
    container_monitor_t containers[MAX_CONTAINERS];
    int container_count;
    time_t last_update;
    int interval;
    int running;
    docker_config_t config;
} monitor_state_t;

void init_monitor_state(monitor_state_t *state, int interval);
void cleanup_monitor_state(monitor_state_t *state);
int get_container_list(monitor_state_t *state);
int get_container_stats(monitor_state_t *state);
void print_container_stats(const monitor_state_t *state);
void print_summary(const monitor_state_t *state);

#endif 