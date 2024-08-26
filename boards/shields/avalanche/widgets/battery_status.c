/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/display/widgets/battery_status.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>

#include "battery_status.h"


static const struct device *display_dev_bat;

static uint16_t *scaled_bitmap_1;
// static uint16_t *scaled_bitmap_2;
// static uint16_t *scaled_bitmap_3;

static uint16_t scale = 5;
static uint16_t font_width = 5;
static uint16_t font_height = 8;
static uint16_t num_color = 0x0000u;
static uint16_t bg_color = 0xFFFFu;

static uint16_t start_x_peripheral_1 = 0;
static uint16_t start_x_peripheral_2 = 155;
static uint16_t start_y = 260;

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct peripheral_battery_state {
    uint8_t source;
    uint8_t level;
};

void print_bitmap(uint16_t *scaled_bitmap, uint16_t bitmap[], uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t scale, uint16_t num_color, uint16_t bg_color) {
	struct display_buffer_descriptor buf_font_desc;

    uint16_t color;
    uint16_t pixel;
    uint16_t font_width_scaled = width * scale;
    uint16_t font_height_scaled = height * scale;
    uint16_t font_buf_size_scaled = font_width_scaled * font_height_scaled;
    uint16_t index = 0;
    for (uint16_t line = 0; line < height; line++) {
        for (uint16_t i = 0; i < scale; i++) {
            for (uint16_t column = 0; column < width; column++) {
                for (uint16_t j = 0; j < scale; j++) {
                    pixel = bitmap[(line*width) + column];
                    if (pixel == 1) {
                        color = num_color;
                    } else {
                        color = bg_color;
                    }
                    *(scaled_bitmap + index) = color;
                    index++;
                }
            }
        }
    }
	buf_font_desc.buf_size = font_buf_size_scaled;
	buf_font_desc.pitch = font_width_scaled;
	buf_font_desc.width = font_width_scaled;
	buf_font_desc.height = font_height_scaled;
    display_write(display_dev_bat, x, y, &buf_font_desc, scaled_bitmap);
}

void print_percentage(uint8_t digit, uint16_t x, uint16_t y, uint16_t scale, uint16_t num_color, uint16_t bg_color) {
    uint16_t dash_bitmap[] = {
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        1, 1, 1, 1, 1,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
    };
    uint16_t f_bitmap[] = {
        1, 1, 1, 1, 1,
        1, 0, 0, 0, 0,
        1, 0, 0, 0, 0,
        1, 0, 0, 0, 0,
        1, 1, 1, 1, 1,
        1, 0, 0, 0, 0,
        1, 0, 0, 0, 0,
        1, 0, 0, 0, 0,
    };
    uint16_t u_bitmap[] = {
        1, 0, 0, 0, 1,
        1, 0, 0, 0, 1,
        1, 0, 0, 0, 1,
        1, 0, 0, 0, 1,
        1, 0, 0, 0, 1,
        1, 0, 0, 0, 1,
        1, 0, 0, 0, 1,
        1, 1, 1, 1, 1,
    };
    uint16_t l_bitmap[] = {
        1, 0, 0, 0, 0,
        1, 0, 0, 0, 0,
        1, 0, 0, 0, 0,
        1, 0, 0, 0, 0,
        1, 0, 0, 0, 0,
        1, 0, 0, 0, 0,
        1, 0, 0, 0, 0,
        1, 1, 1, 1, 1,
    };
    uint16_t percentage_bitmap[] = {
        1, 1, 0, 0, 1,
        1, 1, 0, 1, 0,
        0, 0, 0, 1, 0,
        0, 0, 1, 0, 0,
        0, 0, 1, 0, 0,
        0, 1, 0, 0, 0,
        0, 1, 0, 1, 1,
        1, 0, 0, 1, 1,
    };
    uint16_t num_bitmaps[10][40] = {
        {// zero
            0, 1, 1, 1, 0,
            1, 0, 0, 0, 1,
            1, 0, 0, 0, 1,
            1, 0, 0, 0, 1,
            1, 0, 0, 0, 1,
            1, 0, 0, 0, 1,
            1, 0, 0, 0, 1,
            0, 1, 1, 1, 0
        },
        {// one
            0, 0, 1, 0, 0,
            0, 1, 1, 0, 0,
            0, 0, 1, 0, 0,
            0, 0, 1, 0, 0,
            0, 0, 1, 0, 0,
            0, 0, 1, 0, 0,
            0, 0, 1, 0, 0,
            0, 1, 1, 1, 0
        },
        {// two
            0, 1, 1, 1, 0,
            1, 0, 0, 0, 1,
            0, 0, 0, 0, 1,
            0, 0, 0, 1, 0,
            0, 0, 1, 0, 0,
            0, 1, 0, 0, 0,
            1, 0, 0, 0, 0,
            1, 1, 1, 1, 1
        },
        {// three
            0, 1, 1, 1, 0,
            1, 0, 0, 0, 1,
            0, 0, 0, 0, 1,
            0, 0, 1, 1, 0,
            0, 0, 0, 0, 1,
            0, 0, 0, 0, 1,
            1, 0, 0, 0, 1,
            0, 1, 1, 1, 0
        },
        {// four
            0, 0, 0, 1, 0,
            0, 0, 1, 1, 0,
            0, 1, 0, 1, 0,
            1, 0, 0, 1, 0,
            1, 1, 1, 1, 1,
            0, 0, 0, 1, 0,
            0, 0, 0, 1, 0,
            0, 0, 0, 1, 0
        },
        {// five
            1, 1, 1, 1, 1,
            1, 0, 0, 0, 0,
            1, 0, 0, 0, 0,
            1, 1, 1, 1, 0,
            0, 0, 0, 0, 1,
            0, 0, 0, 0, 1,
            1, 0, 0, 0, 1,
            0, 1, 1, 1, 0
        },
        {// six
            0, 0, 1, 1, 0,
            0, 1, 0, 0, 0,
            1, 0, 0, 0, 0,
            1, 1, 1, 1, 0,
            1, 0, 0, 0, 1,
            1, 0, 0, 0, 1,
            1, 0, 0, 0, 1,
            0, 1, 1, 1, 0
        },
        {// seven
            1, 1, 1, 1, 1,
            0, 0, 0, 0, 1,
            0, 0, 0, 1, 0,
            0, 0, 0, 1, 0,
            0, 0, 1, 0, 0,
            0, 0, 1, 0, 0,
            0, 1, 0, 0, 0,
            0, 1, 0, 0, 0
        },
        {// eight
            0, 1, 1, 1, 0,
            1, 0, 0, 0, 1,
            1, 0, 0, 0, 1,
            0, 1, 1, 1, 0,
            1, 0, 0, 0, 1,
            1, 0, 0, 0, 1,
            1, 0, 0, 0, 1,
            0, 1, 1, 1, 0
        },
        {// nine
            0, 1, 1, 1, 0,
            1, 0, 0, 0, 1,
            1, 0, 0, 0, 1,
            1, 0, 0, 0, 1,
            0, 1, 1, 1, 1,
            0, 0, 0, 0, 1,
            0, 0, 0, 1, 0,
            1, 1, 1, 0, 0
        },
    };

    if (digit == 0) {
        print_bitmap(scaled_bitmap_1, dash_bitmap, x + 0, y, font_width, font_height, scale, num_color, bg_color);
        print_bitmap(scaled_bitmap_1, dash_bitmap, x + 30, y, font_width, font_height, scale, num_color, bg_color);
        print_bitmap(scaled_bitmap_1, percentage_bitmap, x + 60, y, font_width, font_height, scale, num_color, bg_color);
        return;
    }

    if (digit > 99) {
        print_bitmap(scaled_bitmap_1, f_bitmap, x + 0, y, font_width, font_height, scale, num_color, bg_color);
        print_bitmap(scaled_bitmap_1, u_bitmap, x + 30, y, font_width, font_height, scale, num_color, bg_color);
        print_bitmap(scaled_bitmap_1, l_bitmap, x + 60, y, font_width, font_height, scale, num_color, bg_color);
        return;
    }

    uint16_t first_num = digit / 10;
    uint16_t second_num = digit % 10;

    uint16_t width = 5;
    uint16_t height = 8;
    print_bitmap(scaled_bitmap_1, num_bitmaps[first_num], x + 0, y, font_width, font_height, scale, num_color, bg_color);
    print_bitmap(scaled_bitmap_1, num_bitmaps[second_num], x + 30, y, font_width, font_height, scale, num_color, bg_color);
    print_bitmap(scaled_bitmap_1, percentage_bitmap, x + 60, y, font_width, font_height, scale, num_color, bg_color);
}

static void set_battery_symbol(lv_obj_t *widget, struct peripheral_battery_state state) {
    if (state.source == 0) {
        print_percentage(state.level, start_x_peripheral_1, start_y, scale, num_color, bg_color);
    } else {
        print_percentage(state.level, start_x_peripheral_2, start_y, scale, num_color, bg_color);
    }
}

void battery_status_update_cb(struct peripheral_battery_state state) {
    struct zmk_widget_battery_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_symbol(widget->obj, state); }
}

static struct peripheral_battery_state battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev = as_zmk_peripheral_battery_state_changed(eh);
    return (struct peripheral_battery_state){
        .source = ev->source,
        .level = ev->state_of_charge,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct peripheral_battery_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_battery_status, zmk_peripheral_battery_state_changed);



void display_setup_bat(void) {
	display_dev_bat = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev_bat)) {
		LOG_ERR("Device %s not found. Aborting sample.", display_dev_bat->name);
		return;
	}
    
	display_blanking_off(display_dev_bat);
}

int zmk_widget_peripheral_battery_status_init(struct zmk_widget_peripheral_battery_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);

    sys_slist_append(&widgets, &widget->node);

    display_setup_bat();


    uint16_t bitmap_size = (font_width * scale) * (font_height * scale);

    scaled_bitmap_1 = k_malloc(bitmap_size * 2 * sizeof(uint16_t));
    // scaled_bitmap_2 = k_malloc(bitmap_size  * sizeof(uint16_t));
    // scaled_bitmap_3 = k_malloc(bitmap_size  * sizeof(uint16_t));
    
    widget_battery_status_init();

    return 0;
}

lv_obj_t *zmk_widget_peripheral_battery_status_obj(struct zmk_widget_peripheral_battery_status *widget) {
    return widget->obj;
}