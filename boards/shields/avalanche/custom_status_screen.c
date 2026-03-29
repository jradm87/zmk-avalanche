#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include <zmk/display.h>
#include <zmk/keymap.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* 128px / 8px per char = 16 cols, using 4 rows for rain = 32px */
#define RAIN_COLS 16
#define RAIN_ROWS 4

static lv_obj_t *rain_label;
static lv_obj_t *layer_label;
static lv_obj_t *bar;
static bool cursor_vis = true;

static const char *const layer_names[] = {
    "BASE", "CONFIG", "SYS", "GAMING", "---"
};
#define NUM_LAYERS ARRAY_SIZE(layer_names)

static const char charset[] = "01|/\\+-=><01";
#define CHARSET_LEN (sizeof(charset) - 1)

static void refresh_layer(void) {
    uint8_t idx = zmk_keymap_active_layer_index();
    const char *name = (idx < NUM_LAYERS) ? layer_names[idx] : "???";
    char buf[20];
    snprintf(buf, sizeof(buf), "> %s%s", name, cursor_vis ? "_" : " ");
    lv_label_set_text(layer_label, buf);
}

static void rain_timer_cb(lv_timer_t *t) {
    static char buf[RAIN_ROWS * (RAIN_COLS + 1) + 1];
    for (int r = 0; r < RAIN_ROWS; r++) {
        for (int c = 0; c < RAIN_COLS; c++) {
            uint32_t rn = sys_rand32_get();
            /* ~35% chance of a visible character, rest is space */
            buf[r * (RAIN_COLS + 1) + c] =
                (rn % 3 == 0) ? charset[rn % CHARSET_LEN] : ' ';
        }
        buf[r * (RAIN_COLS + 1) + RAIN_COLS] = '\n';
    }
    buf[RAIN_ROWS * (RAIN_COLS + 1)] = '\0';
    lv_label_set_text(rain_label, buf);
}

static void cursor_timer_cb(lv_timer_t *t) {
    cursor_vis = !cursor_vis;
    refresh_layer();
}

static void bar_timer_cb(lv_timer_t *t) {
    static int32_t val = 0;
    static int8_t  dir = 3;
    val += dir;
    if (val >= 100) { val = 100; dir = -3; }
    if (val <= 0)   { val = 0;   dir =  3; }
    lv_bar_set_value(bar, val, LV_ANIM_OFF);
}

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* --- Matrix rain (top 32px) --- */
    rain_label = lv_label_create(scr);
    lv_obj_set_pos(rain_label, 0, 0);
    lv_obj_set_style_text_font(rain_label, &lv_font_unscii_8, 0);
    lv_label_set_long_mode(rain_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(rain_label, 128, 33);

    /* --- Separator line (y=34) --- */
    lv_obj_t *sep = lv_obj_create(scr);
    lv_obj_set_size(sep, 128, 1);
    lv_obj_set_pos(sep, 0, 34);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);

    /* --- Layer name (y=38) --- */
    layer_label = lv_label_create(scr);
    lv_obj_set_pos(layer_label, 2, 38);
    lv_obj_set_style_text_font(layer_label, &lv_font_unscii_8, 0);
    /* solid bg so it's readable over noise if anything leaks */
    lv_obj_set_style_bg_opa(layer_label, LV_OPA_COVER, 0);

    /* --- Scanner bar at bottom --- */
    bar = lv_bar_create(scr);
    lv_obj_set_size(bar, 124, 8);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);

    /* --- Start timers --- */
    lv_timer_create(rain_timer_cb,   180, NULL); /* rain updates */
    lv_timer_create(cursor_timer_cb, 500, NULL); /* cursor blink */
    lv_timer_create(bar_timer_cb,     35, NULL); /* scanner sweep */

    /* Initial draw */
    rain_timer_cb(NULL);
    refresh_layer();

    return scr;
}

/* Update layer label immediately on layer change */
static int layer_changed_handler(const zmk_event_t *eh) {
    refresh_layer();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(avalanche_status_screen, layer_changed_handler);
ZMK_SUBSCRIPTION(avalanche_status_screen, zmk_layer_state_changed);
