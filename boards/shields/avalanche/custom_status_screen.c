#include <zephyr/kernel.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include <zmk/display.h>
#include <zmk/keymap.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/event_manager.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* 128px / 8px per char = 16 chars per row */
#define RAIN_COLS 16
#define RAIN_ROWS 4


static lv_obj_t *rain_label;
static lv_obj_t *layer_label;
static lv_obj_t *recv_label;
static bool cursor_vis = true;
static uint8_t recv_step = 0;

static const char *const layer_names[] = {
    "BASE", "CONFIG", "SYS", "GAMING", "---"
};
#define NUM_LAYERS ARRAY_SIZE(layer_names)

#define IDLE_CHAR '.'

/* ------------------------------------------------------------------ */
/*  Scroll buffer — hex dos keycodes scrollando da direita pra esq.   */
/* ------------------------------------------------------------------ */
#define HEX_BUF (RAIN_ROWS * RAIN_COLS)   /* 64 chars = 4 linhas × 16 */
static char hex_buf[HEX_BUF];
static int64_t last_key_ms = 0;
#define DECAY_AFTER_MS 10000

static const char hexdigits[] = "0123456789ABCDEF";

/* ------------------------------------------------------------------ */
/*  Layer refresh                                                       */
/* ------------------------------------------------------------------ */
static void refresh_layer(void) {
    zmk_keymap_layer_id_t idx = zmk_keymap_highest_layer_active();
    char buf[16];
    snprintf(buf, sizeof(buf), "Layer: %d", (int)idx);
    lv_label_set_text(layer_label, buf);
}

/* ------------------------------------------------------------------ */
/*  Matrix rain timer                                                   */
/* ------------------------------------------------------------------ */
static void rain_timer_cb(lv_timer_t *t) {
    /* após 10s sem digitar, vai puxando chars pra esquerda (2 por tick) */
    if (last_key_ms > 0 && (k_uptime_get() - last_key_ms) > DECAY_AFTER_MS) {
        memmove(hex_buf, hex_buf + 2, HEX_BUF - 2);
        hex_buf[HEX_BUF - 2] = IDLE_CHAR;
        hex_buf[HEX_BUF - 1] = IDLE_CHAR;
        /* se limpou tudo, para o decay */
        bool limpo = true;
        for (int i = 0; i < HEX_BUF; i++) {
            if (hex_buf[i] != IDLE_CHAR) { limpo = false; break; }
        }
        if (limpo) last_key_ms = 0;
    }

    /* monta o buf com quebras de linha para exibição */
    static char buf[RAIN_ROWS * (RAIN_COLS + 1) + 1];
    for (int r = 0; r < RAIN_ROWS; r++) {
        for (int c = 0; c < RAIN_COLS; c++) {
            buf[r * (RAIN_COLS + 1) + c] = hex_buf[r * RAIN_COLS + c];
        }
        buf[r * (RAIN_COLS + 1) + RAIN_COLS] = '\n';
    }
    buf[RAIN_ROWS * (RAIN_COLS + 1)] = '\0';
    lv_label_set_text(rain_label, buf);
}

/* ------------------------------------------------------------------ */
/*  Layer refresh timer                                                 */
/* ------------------------------------------------------------------ */
static void cursor_timer_cb(lv_timer_t *t) {
    refresh_layer();
}

/* ------------------------------------------------------------------ */
/*  Receiving animation (igual ao left/right mas com RECEIVING)        */
/* ------------------------------------------------------------------ */
static void recv_timer_cb(lv_timer_t *t) {
    static const char *frames[] = {
        "<<<RECEIVING>>>",
        " <<RECEIVING>> ",
        "  <RECEIVING>  ",
        " <<RECEIVING>> ",
    };
    recv_step = (recv_step + 1) % ARRAY_SIZE(frames);
    lv_label_set_text(recv_label, frames[recv_step]);
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

    /* Receiving animation at y=54 */
    recv_label = lv_label_create(scr);
    lv_obj_set_pos(recv_label, 0, 54);
    lv_obj_set_style_text_font(recv_label, &lv_font_unscii_8, 0);
    lv_label_set_text(recv_label, "<<<RECEIVING>>>");

    /* Timers */
    lv_timer_create(rain_timer_cb,   300, NULL);
    lv_timer_create(cursor_timer_cb, 600, NULL);
    lv_timer_create(recv_timer_cb,   400, NULL);

    /* Initial draw */
    memset(hex_buf, IDLE_CHAR, HEX_BUF);
    rain_timer_cb(NULL);
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

/* ------------------------------------------------------------------ */
/*  ZMK event listener — injeta hex do keycode na rain ao pressionar   */
/* ------------------------------------------------------------------ */
static void append_hex(uint8_t hi, uint8_t lo) {
    memmove(hex_buf, hex_buf + 4, HEX_BUF - 4);
    hex_buf[HEX_BUF - 4] = '0';
    hex_buf[HEX_BUF - 3] = 'x';
    hex_buf[HEX_BUF - 2] = hexdigits[(lo >> 4) & 0xF];
    hex_buf[HEX_BUF - 1] = hexdigits[lo & 0xF];
    last_key_ms = k_uptime_get();
    (void)hi;
}

static int key_handler(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (!ev || !ev->state) return ZMK_EV_EVENT_BUBBLE;
    append_hex(ev->usage_page & 0xFF, ev->keycode & 0xFF);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(avalanche_key_rain, key_handler);
ZMK_SUBSCRIPTION(avalanche_key_rain, zmk_keycode_state_changed);

