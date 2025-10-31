#include "caps_status.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zephyr/sys/util.h>

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/hid.h>
#include <zmk/hid_indicators.h>
#include <dt-bindings/zmk/hid_usage.h>

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct caps_status_state {
    bool active;
};

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
static inline bool caps_lock_active(zmk_hid_indicators_t indicators) {
    const uint8_t caps_lock_bit = HID_USAGE_LED_CAPS_LOCK - HID_USAGE_LED_NUM_LOCK;
    return (indicators & BIT(caps_lock_bit)) != 0;
}
#endif

static void set_status_symbol(lv_obj_t *label, struct caps_status_state state) {
    lv_label_set_text(label, state.active ? "CAPS" : "");
}

static void caps_status_update_cb(struct caps_status_state state) {
    struct zmk_widget_caps_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_status_symbol(widget->obj, state);
    }
}

static struct caps_status_state caps_status_get_state(const zmk_event_t *eh) {
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    const zmk_hid_indicators_t indicators = zmk_hid_indicators_get_current_profile();
    return (struct caps_status_state){
        .active = caps_lock_active(indicators),
    };
#else
    ARG_UNUSED(eh);
    return (struct caps_status_state){.active = false};
#endif
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_caps_status, struct caps_status_state, caps_status_update_cb,
                            caps_status_get_state)

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
ZMK_SUBSCRIPTION(widget_caps_status, zmk_hid_indicators_changed);
ZMK_SUBSCRIPTION(widget_caps_status, zmk_endpoint_changed);
#endif

int zmk_widget_caps_status_init(struct zmk_widget_caps_status *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);

    sys_slist_append(&widgets, &widget->node);

    widget_caps_status_init();
    return 0;
}

lv_obj_t *zmk_widget_caps_status_obj(struct zmk_widget_caps_status *widget) {
    return widget->obj;
}
