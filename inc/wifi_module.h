#ifndef WIFI_MODULE_H
#define WIFI_MODULE_H

#include <zephyr/types.h>
#include <stdbool.h>

#define MAX_SSID_LEN 33

int init_wifi_manager(void);
int wifi_connect_to_si(void);
char* get_strongest_ssid(void);
int get_wifi_count(void);
int8_t get_best_rssi(void);
bool is_wifi_connected(void);

#endif /* WIFI_MODULE_H */