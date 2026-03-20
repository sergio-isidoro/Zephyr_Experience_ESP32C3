#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/display/cfb.h>
#include <stdio.h>
#include "wifi_module.h"

#define DISPLAY_NODE DT_CHOSEN(zephyr_display)

void display_task(void) {
    const struct device *disp = DEVICE_DT_GET(DISPLAY_NODE);
    if (!device_is_ready(disp)) return;
    cfb_framebuffer_init(disp);
    cfb_framebuffer_invert(disp);

    uint16_t screen_w = cfb_get_display_parameter(disp, CFB_DISPLAY_WIDTH);
    uint16_t screen_h = cfb_get_display_parameter(disp, CFB_DISPLAY_HEIGHT);
    uint8_t font_w, font_h;
    cfb_get_font_size(disp, 0, &font_w, &font_h);

    while (1) {
        char *ssid = get_strongest_ssid();
        int8_t rssi = get_best_rssi();
        bool online = is_wifi_connected();
        char buf[32];

        cfb_framebuffer_clear(disp, false);

        int len;

        len = snprintf(buf, sizeof(buf), "%.10s", ssid);
        cfb_print(disp, buf, (screen_w - len * font_w) / 2, 0);

        len = snprintf(buf, sizeof(buf), "%d", rssi);
        cfb_print(disp, buf, (screen_w - len * font_w) / 2, (screen_h - font_h) / 2);

        len = snprintf(buf, sizeof(buf), "%s", online ? "ONLINE" : "OFFLINE");
        cfb_print(disp, buf, (screen_w - len * font_w) / 2, screen_h - font_h);
        
        cfb_framebuffer_finalize(disp);

        k_msleep(100);
    }
}

K_THREAD_DEFINE(display_tid, 2048, display_task, NULL, NULL, NULL, 7, 0, 0);