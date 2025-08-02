#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/docker_monitor.h"

void print_timestamp(void) {
    time_t now = time(NULL);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    printf("[%s] ", time_str);
}

void print_separator(void) {
    printf("----------------------------------------------------------------\n");
}

void print_header(const char *title) {
    printf("================================================================\n");
    printf("%s\n", title);
    printf("================================================================\n");
}

int is_container_running(const char *status) {
    return strstr(status, "Up") != NULL;
}

void format_duration(time_t duration, char *buffer, size_t size) {
    int days = duration / 86400;
    int hours = (duration % 86400) / 3600;
    int minutes = (duration % 3600) / 60;
    int seconds = duration % 60;
    
    if (days > 0) {
        snprintf(buffer, size, "%dd %02dh %02dm %02ds", days, hours, minutes, seconds);
    } else if (hours > 0) {
        snprintf(buffer, size, "%02dh %02dm %02ds", hours, minutes, seconds);
    } else if (minutes > 0) {
        snprintf(buffer, size, "%02dm %02ds", minutes, seconds);
    } else {
        snprintf(buffer, size, "%02ds", seconds);
    }
} 