#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "../include/docker_monitor.h"
#include "../include/docker_api.h"

volatile int running = 1;

void signal_handler(int sig) {
    printf("\nПолучен сигнал %d, завершение работы...\n", sig);
    running = 0;
}

void print_usage(const char *program_name) {
    printf("Использование: %s [опции]\n", program_name);
    printf("Опции:\n");
    printf("  -h, --help           Показать эту справку\n");
    printf("  -v, --version        Показать версию\n");
    printf("  -i <секунды>         Интервал обновления (по умолчанию: 5)\n");
    printf("  -c <контейнер>       Мониторинг только указанного контейнера\n");
    printf("  -j                   Вывод в JSON формате\n");
    printf("  -s                   Показать только сводку\n");
    printf("  -H <хост>            Docker хост (по умолчанию: localhost)\n");
    printf("  -p <порт>            Docker порт (по умолчанию: 2375)\n");
    printf("  --tls                Использовать TLS соединение\n");
    printf("  --cert <путь>        Путь к сертификату клиента\n");
    printf("  --key <путь>         Путь к ключу клиента\n");
    printf("  --ca <путь>          Путь к CA сертификату\n");
    printf("\nПримеры:\n");
    printf("  %s                    # Мониторинг локальных контейнеров\n", program_name);
    printf("  %s -H 192.168.1.100  # Удаленный хост\n", program_name);
    printf("  %s -H docker.example.com -p 2376 --tls  # TLS соединение\n", program_name);
    printf("  %s -i 10              # Обновление каждые 10 секунд\n", program_name);
    printf("  %s -c my_container    # Только контейнер my_container\n", program_name);
}

void print_version(void) {
    printf("Docker Monitor v1.0.0\n");
    printf("Мониторинг CPU/RAM контейнеров Docker\n");
}

void print_banner(void) {
    printf("================================================================\n");
    printf("                    DOCKER CONTAINER MONITOR                   \n");
    printf("================================================================\n");
}

int main(int argc, char *argv[]) {
    int interval = 5;
    char *target_container = NULL;
    int json_output = 0;
    int summary_only = 0;
    monitor_state_t monitor_state;
    
    memset(&monitor_state, 0, sizeof(monitor_state_t));
    strcpy(monitor_state.config.host, "localhost");
    monitor_state.config.port = 2375;
    monitor_state.config.use_tls = 0;
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "-i") == 0) {
            if (i + 1 < argc) {
                interval = atoi(argv[++i]);
                if (interval <= 0) {
                    fprintf(stderr, "Ошибка: интервал должен быть положительным числом\n");
                    return 1;
                }
            } else {
                fprintf(stderr, "Ошибка: не указан интервал для -i\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-c") == 0) {
            if (i + 1 < argc) {
                target_container = argv[++i];
            } else {
                fprintf(stderr, "Ошибка: не указан контейнер для -c\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-H") == 0) {
            if (i + 1 < argc) {
                strncpy(monitor_state.config.host, argv[++i], sizeof(monitor_state.config.host) - 1);
            } else {
                fprintf(stderr, "Ошибка: не указан хост для -H\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 < argc) {
                monitor_state.config.port = atoi(argv[++i]);
                if (monitor_state.config.port <= 0) {
                    fprintf(stderr, "Ошибка: порт должен быть положительным числом\n");
                    return 1;
                }
            } else {
                fprintf(stderr, "Ошибка: не указан порт для -p\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--tls") == 0) {
            monitor_state.config.use_tls = 1;
        } else if (strcmp(argv[i], "--cert") == 0) {
            if (i + 1 < argc) {
                strncpy(monitor_state.config.cert_path, argv[++i], sizeof(monitor_state.config.cert_path) - 1);
            } else {
                fprintf(stderr, "Ошибка: не указан путь к сертификату для --cert\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--key") == 0) {
            if (i + 1 < argc) {
                strncpy(monitor_state.config.key_path, argv[++i], sizeof(monitor_state.config.key_path) - 1);
            } else {
                fprintf(stderr, "Ошибка: не указан путь к ключу для --key\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--ca") == 0) {
            if (i + 1 < argc) {
                strncpy(monitor_state.config.ca_path, argv[++i], sizeof(monitor_state.config.ca_path) - 1);
            } else {
                fprintf(stderr, "Ошибка: не указан путь к CA для --ca\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-j") == 0) {
            json_output = 1;
        } else if (strcmp(argv[i], "-s") == 0) {
            summary_only = 1;
        } else {
            fprintf(stderr, "Неизвестная опция: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    print_banner();
    printf("Интервал обновления: %d секунд\n", interval);
    printf("Docker хост: %s:%d%s\n", 
           monitor_state.config.host, 
           monitor_state.config.port,
           monitor_state.config.use_tls ? " (TLS)" : "");
    if (target_container) {
        printf("Мониторинг контейнера: %s\n", target_container);
    }
    printf("Нажмите Ctrl+C для остановки\n\n");
    
    if (docker_api_init(&monitor_state.config) != 0) {
        fprintf(stderr, "Ошибка инициализации Docker API\n");
        return 1;
    }
    
    init_monitor_state(&monitor_state, interval);
    
    while (running) {
        if (get_container_list(&monitor_state) == 0) {
            if (get_container_stats(&monitor_state) == 0) {
                if (summary_only) {
                    print_summary(&monitor_state);
                } else {
                    print_container_stats(&monitor_state);
                }
            }
        }
        
        if (running) {
            sleep(interval);
        }
    }
    
    printf("\nЗавершение работы...\n");
    cleanup_monitor_state(&monitor_state);
    docker_api_cleanup();
    
    return 0;
} 