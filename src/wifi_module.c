#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/kernel.h>
#include <string.h>
#include "wifi_module.h"

static char best_ssid[MAX_SSID_LEN] = "None";
static int8_t max_rssi = -127;
static int total_networks = 0;

K_MUTEX_DEFINE(wifi_data_mutex);
K_SEM_DEFINE(scan_done_sem, 0, 1);

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint64_t mgmt_event, struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_WIFI_SCAN_RESULT) {
        const struct wifi_scan_result *res = (const struct wifi_scan_result *)cb->info;
        k_mutex_lock(&wifi_data_mutex, K_FOREVER);
        total_networks++;
        if (res->ssid_length > 0 && res->rssi > max_rssi) {
            max_rssi = res->rssi;
            size_t copy_len = MIN(res->ssid_length, MAX_SSID_LEN - 1);
            memcpy(best_ssid, res->ssid, copy_len);
            best_ssid[copy_len] = '\0';
        }
        k_mutex_unlock(&wifi_data_mutex);
    } 
    else if (mgmt_event == NET_EVENT_WIFI_SCAN_DONE) {
        k_sem_give(&scan_done_sem);
    }
}

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback wifi_done_cb;

int init_wifi_manager(void) {
    net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler, NET_EVENT_WIFI_SCAN_RESULT);
    net_mgmt_init_event_callback(&wifi_done_cb, wifi_mgmt_event_handler, NET_EVENT_WIFI_SCAN_DONE);
    net_mgmt_add_event_callback(&wifi_cb);
    net_mgmt_add_event_callback(&wifi_done_cb);
    return 0;
}

int wifi_connect_to_si(void) {
    struct net_if *iface = net_if_get_default();
    struct wifi_connect_req_params params = {
        .ssid = "SI_WIFI",
        .ssid_length = strlen("SI_WIFI"),
        .psk = "SInternet",
        .psk_length = strlen("SInternet"),
        .channel = WIFI_CHANNEL_ANY,
        .security = WIFI_SECURITY_TYPE_PSK,
        .band = WIFI_FREQ_BAND_2_4_GHZ,
    };
    return net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
}

bool is_wifi_connected(void) {
    struct net_if *iface = net_if_get_default();
    if (!iface || !iface->config.ip.ipv4) return false;

    for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
        struct net_if_addr *ifaddr = &iface->config.ip.ipv4->unicast[i];
        if (ifaddr->is_used && ifaddr->address.family == AF_INET) {
            return true;
        }
    }
    return false;
}

char* get_strongest_ssid(void) {
    struct net_if *iface = net_if_get_default();
    k_mutex_lock(&wifi_data_mutex, K_FOREVER);
    max_rssi = -127; total_networks = 0;
    strcpy(best_ssid, "Scanning...");
    k_mutex_unlock(&wifi_data_mutex);

    if (net_mgmt(NET_REQUEST_WIFI_SCAN, iface, NULL, 0)) return "Error";
    k_sem_take(&scan_done_sem, K_SECONDS(5));
    return best_ssid;
}

int get_wifi_count(void) {
    int val;
    k_mutex_lock(&wifi_data_mutex, K_FOREVER);
    val = total_networks;
    k_mutex_unlock(&wifi_data_mutex);
    return val;
}

int8_t get_best_rssi(void) {
    int8_t val;
    k_mutex_lock(&wifi_data_mutex, K_FOREVER);
    val = max_rssi;
    k_mutex_unlock(&wifi_data_mutex);
    return val;
}