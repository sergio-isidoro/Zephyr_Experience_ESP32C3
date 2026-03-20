#include <zephyr/kernel.h>
#include "wifi_module.h"

int main(void) {
    init_wifi_manager();

    // Pequeno atraso para garantir que o rádio estabilizou
    k_msleep(500);
    
    // Tenta conectar à rede específica
    wifi_connect_to_si();
    
    while (1) {
        k_msleep(1000); // Mantém o main vivo
    }
    return 0;
}