#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/docker_monitor.h"
#include "../include/docker_api.h"

void init_monitor_state(monitor_state_t *state, int interval) {
    state->interval = interval;
    state->running = 1;
    state->last_update = time(NULL);
}

void cleanup_monitor_state(monitor_state_t *state) {
    state->running = 0;
}

int get_container_list(monitor_state_t *state) {
    container_info_t temp_containers[MAX_CONTAINERS];
    int count = docker_get_containers(temp_containers, MAX_CONTAINERS);
    
    if (count < 0) {
        return -1;
    }
    
    state->container_count = count;
    memcpy(state->containers, temp_containers, count * sizeof(container_info_t));
    state->last_update = time(NULL);
    
    return 0;
}

int get_container_stats(monitor_state_t *state) {
    if (!state) {
        return -1;
    }
    
    for (int i = 0; i < state->container_count; i++) {
        container_stats_t stats;
        if (docker_get_container_stats(state->containers[i].info.id, &stats) == 0) {
            state->containers[i].stats = stats;
            
            if (stats.memory_limit > 0) {
                state->containers[i].memory_percent = (double)stats.memory_usage / stats.memory_limit * 100.0;
            } else {
                state->containers[i].memory_percent = 0.0;
            }
            
            state->containers[i].is_running = 1;
        } else {
            state->containers[i].is_running = 0;
        }
    }
    
    return 0;
}

void print_container_stats(const monitor_state_t *state) {
    time_t now = time(NULL);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&now));
    
    printf("\n[%s] Статистика контейнеров (%d контейнеров)\n", time_str, state->container_count);
    printf("----------------------------------------------------------------\n");
    
    for (int i = 0; i < state->container_count; i++) {
        const container_monitor_t *container = &state->containers[i];
        
        printf("Контейнер: %s\n", container->info.name);
        printf("ID: %s\n", container->info.id);
        printf("Образ: %s\n", container->info.image);
        printf("Статус: %s\n", container->info.status);
        
        if (container->is_running) {
            printf("Память: %s / %s (%s)\n", 
                   format_bytes(container->stats.memory_usage),
                   format_bytes(container->stats.memory_limit),
                   format_percentage(container->memory_percent));
            
            printf("Сеть RX: %s | TX: %s\n",
                   format_bytes(container->stats.network_rx_bytes),
                   format_bytes(container->stats.network_tx_bytes));
            
            printf("CPU Usage: %lu | System: %lu\n",
                   container->stats.cpu_usage,
                   container->stats.cpu_system_usage);
        } else {
            printf("Контейнер не запущен\n");
        }
        
        printf("-----\n");
    }
}

void print_summary(const monitor_state_t *state) {
    if (!state) {
        return;
    }
    
    time_t now = time(NULL);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&now));
    
    int running_count = 0;
    uint64_t total_memory = 0;
    uint64_t total_memory_limit = 0;
    
    for (int i = 0; i < state->container_count; i++) {
        if (state->containers[i].is_running) {
            running_count++;
            total_memory += state->containers[i].stats.memory_usage;
            total_memory_limit += state->containers[i].stats.memory_limit;
        }
    }
    
    printf("[%s] Сводка: %d/%d контейнеров запущено | Память: %s / %s\n",
           time_str,
           running_count,
           state->container_count,
           format_bytes(total_memory),
           format_bytes(total_memory_limit));
} 