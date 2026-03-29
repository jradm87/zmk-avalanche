#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include <zmk/display.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/event_manager.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_PERIPHERAL)
#include <zmk/events/split_peripheral_status_changed.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static lv_obj_t *bt_label;
static lv_obj_t *bat_label;
static lv_obj_t *side_label;
static uint8_t   anim_step = 0;

/* ------------------------------------------------------------------ */
/*  Battery bar — called only from event handler, never at init        */
/* ------------------------------------------------------------------ */
static void set_battery(uint8_t pct) {
    int  filled = (pct * 8) / 100;
    char buf[20];
    int  i = 0;

    buf[i++] = '[';
    for (int j = 0; j < 8; j++) buf[i++] = (j < filled) ? '#' : '.';
    buf[i++] = ']';
    buf[i]   = '\0';

    char perc[8];
    snprintf(perc, sizeof(perc), " %3u%%", pct);
    strncat(buf, perc, sizeof(buf) - strlen(buf) - 1);
    lv_label_set_text(bat_label, buf);
}

/* ------------------------------------------------------------------ */
/*  Animated side indicator                                             */
/* ------------------------------------------------------------------ */
static void anim_timer_cb(lv_timer_t *t) {
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

/* ------------------------------------------------------------------ */
/*  Build screen — safe init, no early hardware calls                   */
/* ------------------------------------------------------------------ */
lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Logo row 1 */
    lv_obj_t *logo1 = lv_label_create(scr);
    lv_label_set_text(logo1, " /\\ AVALANCHE");
    lv_obj_set_pos(logo1, 0, 0);

    /* Logo row 2 */
    lv_obj_t *logo2 = lv_label_create(scr);
    lv_label_set_text(logo2, "/  \\ keyboard");
    lv_obj_set_pos(logo2, 0, 14);

    /* Separator */
    lv_obj_t *sep = lv_obj_create(scr);
    lv_obj_set_size(sep, 128, 1);
    lv_obj_set_pos(sep, 0, 26);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);

    /* BT label — safe placeholder, updated by event */
    bt_label = lv_label_create(scr);
    lv_label_set_text(bt_label, "BT: [.........]");
    lv_obj_set_pos(bt_label, 0, 30);

    /* Battery label — safe placeholder, updated by event */
    bat_label = lv_label_create(scr);
    lv_label_set_text(bat_label, "[........]  --%");
    lv_obj_set_pos(bat_label, 0, 42);

    /* Side indicator (animated) */
    side_label = lv_label_create(scr);
    lv_obj_align(side_label, LV_ALIGN_BOTTOM_MID, 0, -2);
#if IS_ENABLED(CONFIG_SHIELD_AVALANCHE_LEFT)
    lv_label_set_text(side_label, "<<< LEFT >>>");
#else
    lv_label_set_text(side_label, ">>> RIGHT <<<");
#endif

    lv_timer_create(anim_timer_cb, 400, NULL);

    return scr;
}

/* ------------------------------------------------------------------ */
/*  Battery event — safe, only runs after BT/battery stack is up       */
/* ------------------------------------------------------------------ */
static int battery_handler(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    if (ev) {
        set_battery(ev->state_of_charge);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(periph_bat, battery_handler);
ZMK_SUBSCRIPTION(periph_bat, zmk_battery_state_changed);

/* ------------------------------------------------------------------ */
/*  BT peripheral connection event                                      */
/* ------------------------------------------------------------------ */
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_PERIPHERAL)
#include <zmk/split/bluetooth/peripheral.h>

static int bt_handler(const zmk_event_t *eh) {
    bool conn = zmk_split_bt_peripheral_is_connected();
    lv_label_set_text(bt_label, conn ? "BT: [CONN  OK]" : "BT: [-- OFF --]");
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(periph_bt, bt_handler);
ZMK_SUBSCRIPTION(periph_bt, zmk_split_peripheral_status_changed);
#endif
