#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <json-c/json.h>
#include <strings.h>
#include "../include/docker_api.h"

static int docker_socket = -1;
static docker_config_t current_config;

static int init_local_socket(void);
static int init_remote_socket(const docker_config_t *config);
static int send_http_request_unix(const char *method, const char *path, char **response);
static int send_http_request_tcp(const char *method, const char *path, char **response);

int docker_api_init(const docker_config_t *config) {
    if (!config) {
        print_error("Некорректная конфигурация");
        return -1;
    }
    
    memcpy(&current_config, config, sizeof(docker_config_t));
    
    if (strcmp(config->host, "localhost") == 0 || strcmp(config->host, "127.0.0.1") == 0) {
        return init_local_socket();
    } else {
        return init_remote_socket(config);
    }
}

static int init_local_socket(void) {
    struct sockaddr_un addr;
    
    if (access(DOCKER_SOCKET, F_OK) == -1) {
        print_error("Docker socket не найден. Убедитесь, что Docker запущен.");
        return -1;
    }
    
    docker_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (docker_socket == -1) {
        print_error("Ошибка создания сокета");
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, DOCKER_SOCKET, sizeof(addr.sun_path) - 1);
    
    if (connect(docker_socket, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        print_error("Ошибка подключения к Docker socket");
        close(docker_socket);
        docker_socket = -1;
        return -1;
    }
    
    return 0;
}

static int init_remote_socket(const docker_config_t *config) {
    struct sockaddr_in addr;
    struct hostent *host;
    
    docker_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (docker_socket == -1) {
        print_error("Ошибка создания TCP сокета");
        return -1;
    }
    
    host = gethostbyname(config->host);
    if (!host) {
        print_error("Не удалось разрешить имя хоста");
        close(docker_socket);
        docker_socket = -1;
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config->port);
    memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);
    
    if (connect(docker_socket, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        print_error("Ошибка подключения к удаленному Docker daemon");
        close(docker_socket);
        docker_socket = -1;
        return -1;
    }
    
    return 0;
}

void docker_api_cleanup(void) {
    if (docker_socket != -1) {
        close(docker_socket);
        docker_socket = -1;
    }
}

static int send_http_request(const char *method, const char *path, char **response) {
    if (strcmp(current_config.host, "localhost") == 0 || strcmp(current_config.host, "127.0.0.1") == 0) {
        return send_http_request_unix(method, path, response);
    } else {
        return send_http_request_tcp(method, path, response);
    }
}

static int send_http_request_unix(const char *method, const char *path, char **response) {
    char request[1024];
    char buffer[4096];
    ssize_t bytes_read;
    size_t total_size = 0;
    char *temp_response = NULL;
    int temp_socket;
    struct sockaddr_un addr;
    
    temp_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (temp_socket == -1) {
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, DOCKER_SOCKET, sizeof(addr.sun_path) - 1);
    
    if (connect(temp_socket, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(temp_socket);
        return -1;
    }
    
    snprintf(request, sizeof(request),
             "%s %s HTTP/1.1\r\n"
             "Host: localhost\r\n"
             "Connection: close\r\n"
             "\r\n",
             method, path);
    
    if (send(temp_socket, request, strlen(request), 0) == -1) {
        close(temp_socket);
        return -1;
    }
    
    while ((bytes_read = recv(temp_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        
        char *new_response = realloc(temp_response, total_size + bytes_read + 1);
        if (!new_response) {
            free(temp_response);
            close(temp_socket);
            return -1;
        }
        
        temp_response = new_response;
        strcpy(temp_response + total_size, buffer);
        total_size += bytes_read;
    }
    
    close(temp_socket);
    
    if (temp_response) {
        char *body_start = strstr(temp_response, "\r\n\r\n");
        if (body_start) {
            body_start += 4;
            *response = malloc(strlen(body_start) + 1);
            if (*response) {
                strcpy(*response, body_start);
            }
            free(temp_response);
        } else {
            *response = temp_response;
        }
    }
    
    return 0;
}

static int send_http_request_tcp(const char *method, const char *path, char **response) {
    char request[1024];
    char buffer[4096];
    ssize_t bytes_read;
    size_t total_size = 0;
    char *temp_response = NULL;
    int temp_socket;
    struct sockaddr_in addr;
    struct hostent *host;
    
    temp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (temp_socket == -1) {
        return -1;
    }
    
    host = gethostbyname(current_config.host);
    if (!host) {
        close(temp_socket);
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(current_config.port);
    memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);
    
    if (connect(temp_socket, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(temp_socket);
        return -1;
    }
    
    snprintf(request, sizeof(request),
             "%s %s HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "Connection: close\r\n"
             "\r\n",
             method, path, current_config.host, current_config.port);
    
    if (send(temp_socket, request, strlen(request), 0) == -1) {
        close(temp_socket);
        return -1;
    }
    
    while ((bytes_read = recv(temp_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        
        char *new_response = realloc(temp_response, total_size + bytes_read + 1);
        if (!new_response) {
            free(temp_response);
            close(temp_socket);
            return -1;
        }
        
        temp_response = new_response;
        strcpy(temp_response + total_size, buffer);
        total_size += bytes_read;
    }
    
    close(temp_socket);
    
    if (temp_response) {
        char *body_start = strstr(temp_response, "\r\n\r\n");
        if (body_start) {
            body_start += 4;
            *response = malloc(strlen(body_start) + 1);
            if (*response) {
                strcpy(*response, body_start);
            }
            free(temp_response);
        } else {
            *response = temp_response;
        }
    }
    
    return 0;
}

int docker_get_containers(container_info_t *containers, int max_count) {
    char *response = NULL;
    int result = -1;
    
    if (send_http_request("GET", "/containers/json", &response) == 0) {
        result = docker_parse_container_list(response, containers, max_count);
        free(response);
    }
    
    return result;
}

int docker_get_container_stats(const char *container_id, container_stats_t *stats) {
    char path[256];
    char *response = NULL;
    int result = -1;
    
    snprintf(path, sizeof(path), "/containers/%s/stats?stream=false", container_id);
    
    if (send_http_request("GET", path, &response) == 0) {
        result = docker_parse_container_stats(response, stats);
        free(response);
    }
    
    return result;
}

int docker_parse_container_list(const char *json_data, container_info_t *containers, int max_count) {
    json_object *root, *container, *names, *name;
    int count = 0;
    
    if (!json_data || !containers) {
        print_error("Некорректные параметры для парсинга");
        return -1;
    }
    
    root = json_tokener_parse(json_data);
    if (!root) {
        print_error("Ошибка парсинга JSON");
        return -1;
    }
    
    if (!json_object_is_type(root, json_type_array)) {
        print_error("Ожидался JSON массив");
        json_object_put(root);
        return -1;
    }
    
    for (int i = 0; i < json_object_array_length(root) && count < max_count; i++) {
        container = json_object_array_get_idx(root, i);
        if (!container) continue;
        
        json_object *id_obj, *names_obj, *image_obj, *status_obj, *created_obj;
        
        if (json_object_object_get_ex(container, "Id", &id_obj) &&
            json_object_object_get_ex(container, "Names", &names_obj) &&
            json_object_object_get_ex(container, "Image", &image_obj) &&
            json_object_object_get_ex(container, "Status", &status_obj) &&
            json_object_object_get_ex(container, "Created", &created_obj)) {
            
            const char *id_str = json_object_get_string(id_obj);
            const char *image_str = json_object_get_string(image_obj);
            const char *status_str = json_object_get_string(status_obj);
            
            if (id_str && image_str && status_str) {
                strncpy(containers[count].id, id_str, sizeof(containers[count].id) - 1);
                containers[count].id[sizeof(containers[count].id) - 1] = '\0';
                
                if (json_object_array_length(names_obj) > 0) {
                    name = json_object_array_get_idx(names_obj, 0);
                    if (name) {
                        const char *name_str = json_object_get_string(name);
                        if (name_str) {
                            if (name_str[0] == '/') name_str++;
                            strncpy(containers[count].name, name_str, sizeof(containers[count].name) - 1);
                            containers[count].name[sizeof(containers[count].name) - 1] = '\0';
                        } else {
                            strcpy(containers[count].name, "unknown");
                        }
                    } else {
                        strcpy(containers[count].name, "unknown");
                    }
                } else {
                    strcpy(containers[count].name, "unknown");
                }
                
                strncpy(containers[count].image, image_str, sizeof(containers[count].image) - 1);
                containers[count].image[sizeof(containers[count].image) - 1] = '\0';
                
                strncpy(containers[count].status, status_str, sizeof(containers[count].status) - 1);
                containers[count].status[sizeof(containers[count].status) - 1] = '\0';
                
                containers[count].created = json_object_get_int64(created_obj);
                containers[count].last_seen = time(NULL);
                
                count++;
            }
        }
    }
    
    json_object_put(root);
    return count;
}

int docker_parse_container_stats(const char *json_data, container_stats_t *stats) {
    json_object *root, *cpu_stats, *memory_stats, *networks, *blkio_stats;
    json_object *cpu_usage, *memory_usage, *memory_limit;
    
    if (!json_data || !stats) {
        print_error("Некорректные параметры для парсинга статистики");
        return -1;
    }
    
    root = json_tokener_parse(json_data);
    if (!root) {
        print_error("Ошибка парсинга JSON статистики");
        return -1;
    }
    
    memset(stats, 0, sizeof(container_stats_t));
    stats->timestamp = time(NULL);
    
    if (json_object_object_get_ex(root, "cpu_stats", &cpu_stats) && cpu_stats &&
        json_object_object_get_ex(cpu_stats, "cpu_usage", &cpu_usage) && cpu_usage) {
        
        json_object *total_usage, *system_cpu_usage;
        if (json_object_object_get_ex(cpu_usage, "total_usage", &total_usage) && total_usage) {
            stats->cpu_usage = json_object_get_uint64(total_usage);
        }
        if (json_object_object_get_ex(cpu_stats, "system_cpu_usage", &system_cpu_usage) && system_cpu_usage) {
            stats->cpu_system_usage = json_object_get_uint64(system_cpu_usage);
        }
    }
    
    if (json_object_object_get_ex(root, "memory_stats", &memory_stats) && memory_stats) {
        if (json_object_object_get_ex(memory_stats, "usage", &memory_usage) && memory_usage) {
            stats->memory_usage = json_object_get_uint64(memory_usage);
        }
        if (json_object_object_get_ex(memory_stats, "limit", &memory_limit) && memory_limit) {
            stats->memory_limit = json_object_get_uint64(memory_limit);
        }
    }
    
    if (json_object_object_get_ex(root, "networks", &networks) && networks) {
        json_object *eth0;
        if (json_object_object_get_ex(networks, "eth0", &eth0) && eth0) {
            json_object *rx_bytes, *tx_bytes;
            if (json_object_object_get_ex(eth0, "rx_bytes", &rx_bytes) && rx_bytes) {
                stats->network_rx_bytes = json_object_get_uint64(rx_bytes);
            }
            if (json_object_object_get_ex(eth0, "tx_bytes", &tx_bytes) && tx_bytes) {
                stats->network_tx_bytes = json_object_get_uint64(tx_bytes);
            }
        }
    }
    
    json_object_put(root);
    return 0;
}

char *format_bytes(uint64_t bytes) {
    static char buffer[32];
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = bytes;
    
    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }
    
    if (unit_index == 0) {
        snprintf(buffer, sizeof(buffer), "%lu %s", bytes, units[unit_index]);
    } else {
        snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unit_index]);
    }
    
    return buffer;
}

char *format_percentage(double value) {
    static char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.2f%%", value);
    return buffer;
}

void print_error(const char *message) {
    fprintf(stderr, "Ошибка: %s\n", message);
} 