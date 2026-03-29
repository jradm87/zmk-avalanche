#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include <zmk/display.h>
#include <zmk/keymap.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* 128px / 8px per char = 16 chars per row */
#define RAIN_COLS 16
#define RAIN_ROWS 4

/* Scanner bar: 14 slots inside brackets = "[" + 14 chars + "]" */
#define BAR_LEN 14

static lv_obj_t *rain_label;
static lv_obj_t *layer_label;
static lv_obj_t *bar_label;
static bool cursor_vis = true;

static const char *const layer_names[] = {
    "BASE", "CONFIG", "SYS", "GAMING", "---"
};
#define NUM_LAYERS ARRAY_SIZE(layer_names)

static const char charset[] = "01|/\\+-=><01";
#define CHARSET_LEN (sizeof(charset) - 1)

/* ------------------------------------------------------------------ */
/*  Layer refresh                                                       */
/* ------------------------------------------------------------------ */
static void refresh_layer(void) {
    zmk_keymap_layer_id_t idx = zmk_keymap_highest_layer_active();
    const char *name = zmk_keymap_layer_name(idx);
    if (!name || name[0] == '\0') {
        name = (idx < NUM_LAYERS) ? layer_names[idx] : "???";
    }
    char buf[20];
    snprintf(buf, sizeof(buf), "> %s%s", name, cursor_vis ? "_" : " ");
    lv_label_set_text(layer_label, buf);
}

/* ------------------------------------------------------------------ */
/*  Matrix rain timer                                                   */
/* ------------------------------------------------------------------ */
static void rain_timer_cb(lv_timer_t *t) {
    static char buf[RAIN_ROWS * (RAIN_COLS + 1) + 1];
    for (int r = 0; r < RAIN_ROWS; r++) {
        for (int c = 0; c < RAIN_COLS; c++) {
            uint32_t rn = sys_rand32_get();
            buf[r * (RAIN_COLS + 1) + c] =
                (rn % 3 == 0) ? charset[rn % CHARSET_LEN] : ' ';
        }
        buf[r * (RAIN_COLS + 1) + RAIN_COLS] = '\n';
    }
    buf[RAIN_ROWS * (RAIN_COLS + 1)] = '\0';
    lv_label_set_text(rain_label, buf);
}

/* ------------------------------------------------------------------ */
/*  Cursor blink timer                                                  */
/* ------------------------------------------------------------------ */
static void cursor_timer_cb(lv_timer_t *t) {
    cursor_vis = !cursor_vis;
    refresh_layer();
}

/* ------------------------------------------------------------------ */
/*  Scanner bar timer  (text-based: [>>>         ] sweeping L<->R)     */
/* ------------------------------------------------------------------ */
static void bar_timer_cb(lv_timer_t *t) {
    static int pos = 0;
    static int dir = 1;

    char buf[BAR_LEN + 3]; /* '[' + BAR_LEN + ']' + '\0' */
    buf[0] = '[';
    for (int i = 0; i < BAR_LEN; i++) {
        int dist = i - pos;
        if (dist >= 0 && dist < 3) {
            buf[i + 1] = '>';
        } else {
            buf[i + 1] = ' ';
        }
    }
    buf[BAR_LEN + 1] = ']';
    buf[BAR_LEN + 2] = '\0';
    lv_label_set_text(bar_label, buf);

    pos += dir;
    if (pos >= BAR_LEN - 2) dir = -1;
    if (pos <= 0)            dir =  1;
}

/* ------------------------------------------------------------------ */
/*  Build screen                                                        */
/* ------------------------------------------------------------------ */
lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Matrix rain — top 32 px (4 rows × 8 px) */
    rain_label = lv_label_create(scr);
    lv_obj_set_pos(rain_label, 0, 0);
    lv_obj_set_style_text_font(rain_label, &lv_font_unscii_8, 0);
    lv_label_set_long_mode(rain_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(rain_label, 128, 33);

    /* Separator line at y=34 */
    lv_obj_t *sep = lv_obj_create(scr);
    lv_obj_set_size(sep, 128, 1);
    lv_obj_set_pos(sep, 0, 34);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);

    /* Layer name at y=38 */
    layer_label = lv_label_create(scr);
    lv_obj_set_pos(layer_label, 2, 38);
    lv_obj_set_style_text_font(layer_label, &lv_font_unscii_8, 0);

    /* Scanner bar at y=54 */
    bar_label = lv_label_create(scr);
    lv_obj_set_pos(bar_label, 0, 54);
    lv_obj_set_style_text_font(bar_label, &lv_font_unscii_8, 0);

    /* Timers */
    lv_timer_create(rain_timer_cb,   180, NULL);
    lv_timer_create(cursor_timer_cb, 500, NULL);
    lv_timer_create(bar_timer_cb,     60, NULL);

    /* Initial draw */
    rain_timer_cb(NULL);
    bar_timer_cb(NULL);
    refresh_layer();

    return scr;
}

/* ------------------------------------------------------------------ */
/*  ZMK event listener — update layer label on layer change            */
/* ------------------------------------------------------------------ */
static int layer_changed_handler(const zmk_event_t *eh) {
    refresh_layer();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(avalanche_status_screen, layer_changed_handler);
ZMK_SUBSCRIPTION(avalanche_status_screen, zmk_layer_state_changed);
