#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include <zmk/display.h>
#include <zmk/keymap.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/battery_state_changed.h>
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
static lv_obj_t *bat_label[NUM_SIDES];
static lv_obj_t *bat_bar[NUM_SIDES];
static lv_obj_t *conn_label;

static const char *const side_prefix[NUM_SIDES] = {"L", "R"};

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
/*  Battery refresh — updates only the side that changed                */
/* ------------------------------------------------------------------ */
static void refresh_battery(uint8_t side) {
    char buf[8];
    if (bat_pct[side] == BATTERY_UNKNOWN) {
        snprintf(buf, sizeof(buf), "%s: --", side_prefix[side]);
        lv_bar_set_value(bat_bar[side], 0, LV_ANIM_OFF);
    } else {
        snprintf(buf, sizeof(buf), "%s:%3u%%", side_prefix[side], bat_pct[side]);
        lv_bar_set_value(bat_bar[side], bat_pct[side], LV_ANIM_OFF);
    }
    lv_label_set_text(bat_label[side], buf);
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

    /* Battery rows: label + thin bar, left half / right half of screen */
    for (int i = 0; i < NUM_SIDES; i++) {
        int y = 18 + (i * 16);

        bat_label[i] = lv_label_create(scr);
        lv_obj_set_style_text_font(bat_label[i], &lv_font_unscii_8, 0);
        lv_obj_set_pos(bat_label[i], 0, y);

        bat_bar[i] = lv_bar_create(scr);
        lv_obj_set_size(bat_bar[i], 60, 6);
        lv_obj_set_pos(bat_bar[i], 60, y + 1);
        lv_bar_set_range(bat_bar[i], 0, 100);
        lv_obj_set_style_bg_opa(bat_bar[i], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(bat_bar[i], 1, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bat_bar[i], LV_OPA_COVER, LV_PART_INDICATOR);

        refresh_battery(i);
    }

    /* Layer name */
    layer_label = lv_label_create(scr);
    lv_obj_set_style_text_font(layer_label, &lv_font_unscii_8, 0);
    lv_obj_set_pos(layer_label, 0, 42);

    /* Connection status */
    conn_label = lv_label_create(scr);
    lv_obj_set_style_text_font(conn_label, &lv_font_unscii_8, 0);
    lv_obj_set_pos(conn_label, 0, 54);

    /* Slow timer just to age out stale connection status, no busy polling */
    lv_timer_create(conn_timer_cb, 2000, NULL);

    refresh_layer();
    refresh_conn();

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
    refresh_battery(ev->source);
    refresh_conn();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(avalanche_battery_screen, battery_changed_handler);
ZMK_SUBSCRIPTION(avalanche_battery_screen, zmk_peripheral_battery_state_changed);
