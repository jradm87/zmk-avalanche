#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>

/* Peripheral = ZMK_SPLIT enabled AND not central role */
#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#define IS_PERIPHERAL 1
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/events/split_peripheral_status_changed.h>
#else
#define IS_PERIPHERAL 0
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static lv_obj_t *bt_label;
static lv_obj_t *side_label;
static uint8_t   anim_step = 0;

static void anim_timer_cb(lv_timer_t *t) {
    /* Poll BT status every 400ms */
#if IS_PERIPHERAL
    bool conn = zmk_split_bt_peripheral_is_connected();
    lv_label_set_text(bt_label, conn ? "BT: [CONN  OK]" : "BT: [-- OFF --]");
#endif

    /* Animate side indicator */
#if IS_ENABLED(CONFIG_SHIELD_AVALANCHE_LEFT)
    static const char *frames[] = {
        "<<< LEFT >>>",
        " << LEFT >> ",
        "  < LEFT >  ",
        " << LEFT >> ",
    };
#else
    static const char *frames[] = {
        ">>> RIGHT <<<",
        " >> RIGHT << ",
        "  > RIGHT <  ",
        " >> RIGHT << ",
    };
#endif
    anim_step = (anim_step + 1) % ARRAY_SIZE(frames);
    lv_label_set_text(side_label, frames[anim_step]);
}

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *scr = lv_obj_create(NULL);

    lv_obj_t *logo1 = lv_label_create(scr);
    lv_obj_set_style_text_font(logo1, &lv_font_unscii_8, 0);
    lv_label_set_text(logo1, " /\\ AVALANCHE");
    lv_obj_set_pos(logo1, 0, 0);

    lv_obj_t *logo2 = lv_label_create(scr);
    lv_obj_set_style_text_font(logo2, &lv_font_unscii_8, 0);
    lv_label_set_text(logo2, "/  \\ keyboard");
    lv_obj_set_pos(logo2, 0, 12);

    bt_label = lv_label_create(scr);
    lv_obj_set_style_text_font(bt_label, &lv_font_unscii_8, 0);
    lv_label_set_text(bt_label, "BT: [-- OFF --]");
    lv_obj_set_pos(bt_label, 0, 30);

    side_label = lv_label_create(scr);
    lv_obj_set_style_text_font(side_label, &lv_font_unscii_8, 0);
    lv_obj_align(side_label, LV_ALIGN_BOTTOM_MID, 0, -2);
#if IS_ENABLED(CONFIG_SHIELD_AVALANCHE_LEFT)
    lv_label_set_text(side_label, "<<< LEFT >>>");
#else
    lv_label_set_text(side_label, ">>> RIGHT <<<");
#endif

    lv_timer_create(anim_timer_cb, 400, NULL);

    return scr;
}

#if IS_PERIPHERAL
static int bt_handler(const zmk_event_t *eh) {
    bool conn = zmk_split_bt_peripheral_is_connected();
    lv_label_set_text(bt_label, conn ? "BT: [CONN  OK]" : "BT: [-- OFF --]");
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(periph_bt, bt_handler);
ZMK_SUBSCRIPTION(periph_bt, zmk_split_peripheral_status_changed);
#endif
