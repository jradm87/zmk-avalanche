#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include <zmk/display.h>
#include <zmk/keymap.h>
#include <zmk/hid_indicators.h>
#include <dt-bindings/zmk/hid_usage.h>
#include <zmk/usb.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static const char *const layer_names[] = {
    "BASE", "CONFIG", "SYS", "GAMING", "---"
};
#define NUM_LAYERS ARRAY_SIZE(layer_names)

/* battery state per peripheral: 0 = left, 1 = right */
#define NUM_SIDES 2
#define BATTERY_UNKNOWN 0xFF
#define STALE_AFTER_MS (6 * 60 * 1000) /* battery reports every 300s, give 2x margin */

static uint8_t bat_pct[NUM_SIDES] = {BATTERY_UNKNOWN, BATTERY_UNKNOWN};
static int64_t bat_seen_ms[NUM_SIDES] = {0, 0};

static lv_obj_t *layer_label;
static lv_obj_t *battery_label;
static lv_obj_t *conn_label;
static lv_obj_t *caps_bg;
static lv_obj_t *caps_label;
static lv_obj_t *usb_label;

/* ------------------------------------------------------------------ */
/*  Layer refresh                                                       */
/* ------------------------------------------------------------------ */
static void refresh_layer(void) {
    zmk_keymap_layer_id_t idx = zmk_keymap_highest_layer_active();
    const char *name = (idx < NUM_LAYERS) ? layer_names[idx] : "?";
    char buf[24];
    snprintf(buf, sizeof(buf), "Layer: %s", name);
    lv_label_set_text(layer_label, buf);
}

/* ------------------------------------------------------------------ */
/*  Battery refresh — single line, both sides                          */
/* ------------------------------------------------------------------ */
static void refresh_battery(void) {
    char l[8], r[8];
    if (bat_pct[0] == BATTERY_UNKNOWN) {
        snprintf(l, sizeof(l), "L:--");
    } else {
        snprintf(l, sizeof(l), "L:%u%%", bat_pct[0]);
    }
    if (bat_pct[1] == BATTERY_UNKNOWN) {
        snprintf(r, sizeof(r), "R:--");
    } else {
        snprintf(r, sizeof(r), "R:%u%%", bat_pct[1]);
    }
    char buf[24];
    snprintf(buf, sizeof(buf), "%s   %s", l, r);
    lv_label_set_text(battery_label, buf);
}

/* ------------------------------------------------------------------ */
/*  Connection status — derived from battery report freshness           */
/* ------------------------------------------------------------------ */
static void refresh_conn(void) {
    int64_t now = k_uptime_get();
    bool left_ok = bat_seen_ms[0] != 0 && (now - bat_seen_ms[0]) < STALE_AFTER_MS;
    bool right_ok = bat_seen_ms[1] != 0 && (now - bat_seen_ms[1]) < STALE_AFTER_MS;
    char buf[24];
    snprintf(buf, sizeof(buf), "L:%s R:%s", left_ok ? "OK" : "--", right_ok ? "OK" : "--");
    lv_label_set_text(conn_label, buf);
}

static void conn_timer_cb(lv_timer_t *t) {
    refresh_conn();
}

/* ------------------------------------------------------------------ */
/*  USB status — enumerated as HID vs power-only                        */
/* ------------------------------------------------------------------ */
static void refresh_usb(enum zmk_usb_conn_state state) {
    lv_label_set_text(usb_label, state == ZMK_USB_CONN_HID ? "USB: HID" : "USB: PWR");
}

/* ------------------------------------------------------------------ */
/*  Caps lock refresh — inverted highlight when active                  */
/* ------------------------------------------------------------------ */
static void refresh_caps(bool caps_on) {
    if (caps_on) {
        lv_obj_set_style_bg_opa(caps_bg, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(caps_label, lv_color_white(), 0);
    } else {
        lv_obj_set_style_bg_opa(caps_bg, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(caps_label, lv_color_black(), 0);
    }
}

/* ------------------------------------------------------------------ */
/*  Build screen                                                        */
/* ------------------------------------------------------------------ */
lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_unscii_8, 0);
    lv_label_set_text(title, "AVALANCHE");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    /* Battery line: both sides, single label so it never overflows/clips */
    battery_label = lv_label_create(scr);
    lv_obj_set_style_text_font(battery_label, &lv_font_unscii_8, 0);
    lv_obj_set_pos(battery_label, 0, 16);

    /* Layer name */
    layer_label = lv_label_create(scr);
    lv_obj_set_style_text_font(layer_label, &lv_font_unscii_8, 0);
    lv_obj_set_pos(layer_label, 0, 28);

    /* Caps lock indicator — background rect toggled on/off behind the text */
    caps_bg = lv_obj_create(scr);
    lv_obj_set_size(caps_bg, 72, 10);
    lv_obj_set_pos(caps_bg, 0, 38);
    lv_obj_set_style_border_width(caps_bg, 0, 0);
    lv_obj_set_style_pad_all(caps_bg, 0, 0);
    lv_obj_set_style_bg_color(caps_bg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(caps_bg, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(caps_bg, LV_OBJ_FLAG_SCROLLABLE);

    caps_label = lv_label_create(caps_bg);
    lv_obj_set_style_text_font(caps_label, &lv_font_unscii_8, 0);
    lv_label_set_text(caps_label, "CAPS LOCK");
    lv_obj_align(caps_label, LV_ALIGN_LEFT_MID, 0, 0);

    /* Connection status */
    conn_label = lv_label_create(scr);
    lv_obj_set_style_text_font(conn_label, &lv_font_unscii_8, 0);
    lv_obj_set_pos(conn_label, 0, 48);

    /* USB status */
    usb_label = lv_label_create(scr);
    lv_obj_set_style_text_font(usb_label, &lv_font_unscii_8, 0);
    lv_obj_set_pos(usb_label, 0, 58);

    /* Slow timer just to age out stale connection status, no busy polling */
    lv_timer_create(conn_timer_cb, 2000, NULL);

    refresh_battery();
    refresh_layer();
    refresh_conn();
    refresh_caps(zmk_hid_indicators_get_current_profile() & HID_USAGE_LED_CAPS_LOCK);
    refresh_usb(zmk_usb_get_conn_state());

    return scr;
}

/* ------------------------------------------------------------------ */
/*  ZMK event listener — layer change                                   */
/* ------------------------------------------------------------------ */
static int layer_changed_handler(const zmk_event_t *eh) {
    refresh_layer();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(avalanche_status_screen, layer_changed_handler);
ZMK_SUBSCRIPTION(avalanche_status_screen, zmk_layer_state_changed);

/* ------------------------------------------------------------------ */
/*  ZMK event listener — peripheral battery update                      */
/* ------------------------------------------------------------------ */
static int battery_changed_handler(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);
    if (!ev || ev->source >= NUM_SIDES) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    bat_pct[ev->source] = ev->state_of_charge;
    bat_seen_ms[ev->source] = k_uptime_get();
    refresh_battery();
    refresh_conn();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(avalanche_battery_screen, battery_changed_handler);
ZMK_SUBSCRIPTION(avalanche_battery_screen, zmk_peripheral_battery_state_changed);

/* ------------------------------------------------------------------ */
/*  ZMK event listener — caps lock (HID indicators) update              */
/* ------------------------------------------------------------------ */
static int hid_indicators_changed_handler(const zmk_event_t *eh) {
    const struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);
    if (!ev) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    refresh_caps(ev->indicators & HID_USAGE_LED_CAPS_LOCK);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(avalanche_caps_screen, hid_indicators_changed_handler);
ZMK_SUBSCRIPTION(avalanche_caps_screen, zmk_hid_indicators_changed);

/* ------------------------------------------------------------------ */
/*  ZMK event listener — USB connection state (HID vs power-only)       */
/* ------------------------------------------------------------------ */
static int usb_conn_state_changed_handler(const zmk_event_t *eh) {
    const struct zmk_usb_conn_state_changed *ev = as_zmk_usb_conn_state_changed(eh);
    if (!ev) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    refresh_usb(ev->conn_state);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(avalanche_usb_screen, usb_conn_state_changed_handler);
ZMK_SUBSCRIPTION(avalanche_usb_screen, zmk_usb_conn_state_changed);
