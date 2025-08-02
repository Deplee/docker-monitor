#ifndef DOCKER_API_H
#define DOCKER_API_H

#include "docker_monitor.h"
#include <json-c/json.h>

typedef struct {
    char *response;
    size_t size;
} http_response_t;

int docker_api_init(const docker_config_t *config);
void docker_api_cleanup(void);
int docker_get_containers(container_info_t *containers, int max_count);
int docker_get_container_stats(const char *container_id, container_stats_t *stats);
int docker_parse_container_list(const char *json_data, container_info_t *containers, int max_count);
int docker_parse_container_stats(const char *json_data, container_stats_t *stats);
char *format_bytes(uint64_t bytes);
char *format_percentage(double value);
void print_error(const char *message);

#endif
