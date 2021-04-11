/*
 * Copyright (c) 2021 Nicholas Winans
 *
 * SPDX-License-Identifier: MIT
 */

#include <init.h>
#include <stdio.h>
#include <zephyr.h>

#include <device.h>
#include <drivers/gpio.h>
#include <settings/settings.h>
#include <usb/class/usb_hid.h>
#include <usb/usb_device.h>

#define LOG_LEVEL LOG_LEVEL_INF
LOG_MODULE_REGISTER(main);

static bool configured;
static const struct device *hdev;
static ATOMIC_DEFINE(hid_ep_in_busy, 1);

#define HID_EP_BUSY_FLAG 0
#define REPORT_ID_1 0x01

struct report {
  uint8_t id;
  uint8_t value[64];
};

static uint8_t led_value = 0;

static const uint8_t hid_report_desc[] = {
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
    HID_USAGE(HID_USAGE_GEN_DESKTOP_UNDEFINED),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),

    // Define HID Input Descriptor
    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX16(0xFF, 0x00),
    HID_REPORT_ID(REPORT_ID_1),
    HID_REPORT_SIZE(8),                         // Bits per report (1 byte)
    HID_REPORT_COUNT(64),                       // 64 reports for input
    HID_USAGE(HID_USAGE_GEN_DESKTOP_UNDEFINED), // Undefined or raw usage
    HID_INPUT(0x02),

    // Define HID Output Descriptor
    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX16(0xFF, 0x00),
    HID_REPORT_ID(REPORT_ID_1),
    HID_REPORT_SIZE(8),                         // Bits per report (1 byte)
    HID_REPORT_COUNT(64),                       // 64 reports for output
    HID_USAGE(HID_USAGE_GEN_DESKTOP_UNDEFINED), // Undefined or raw usage
    HID_OUTPUT(0x02),

    HID_END_COLLECTION,
};

static void set_leds(uint8_t led_status) {
  const struct device *gpio0 = device_get_binding("GPIO_0");
  gpio_pin_set(gpio0, 13, led_status & 1);
  gpio_pin_set(gpio0, 14, (led_status >> 1) & 1);
  gpio_pin_set(gpio0, 15, (led_status >> 2) & 1);
  gpio_pin_set(gpio0, 16, (led_status >> 3) & 1);
}

static void send_in_report(struct k_work *work) {
  int ret, wrote;

  if (!atomic_test_and_set_bit(hid_ep_in_busy, HID_EP_BUSY_FLAG)) {
    static struct report report_1 = {
        .id = REPORT_ID_1,
    };
    report_1.value[0] = led_value;

    ret =
        hid_int_ep_write(hdev, (uint8_t *)&report_1, sizeof(report_1), &wrote);
    if (ret != 0) {
      LOG_ERR("Failed to submit in report");
    } else {
      LOG_DBG("In report submitted");
    }
  } else {
    LOG_DBG("HID IN endpoint busy");
  }
}

static void int_in_ready_cb(const struct device *dev) {
  if (!atomic_test_and_clear_bit(hid_ep_in_busy, HID_EP_BUSY_FLAG)) {
    LOG_WRN("IN endpoint callback without preceding buffer write");
  }
}

K_WORK_DEFINE(in_report, send_in_report);

static void int_out_ready_cb(const struct device *dev) {
  struct report report_2;
  int ret, read;
  ret = hid_int_ep_read(dev, (uint8_t *)&report_2, sizeof(report_2), &read);

  if (ret == 0 && read > 0) {
    switch (report_2.value[0]) {
    case 1: // Set LED report
      LOG_DBG("Got set LED report");
      led_value = report_2.value[1];
      set_leds(led_value);
      break;
    case 2: // Save LED setting report
      LOG_DBG("Got save LED setting report");
      settings_save_one("led/bits", &led_value, sizeof(led_value));
      break;
    case 3: // Read LED setting report
      LOG_DBG("Got read LED value report");
      k_work_submit(&in_report);
      break;
    }
  } else {
    LOG_ERR("Failed to read out report");
  }
}

static const struct hid_ops ops = {
    .int_in_ready = int_in_ready_cb,
    .int_out_ready = int_out_ready_cb,
};

static void status_cb(enum usb_dc_status_code status, const uint8_t *param) {
  switch (status) {
  case USB_DC_RESET:
    // Connection reset, needs to be reconfigured
    configured = false;
    break;
  case USB_DC_CONFIGURED:
    // Device configured with host
    if (!configured) {
      int_in_ready_cb(hdev);
      configured = true;
    }
    break;
  case USB_DC_SOF:
    break;
  default:
    LOG_DBG("Unhandled USB status: %u", status);
    break;
  }
}

static void init_gpio() {
  const struct device *gpio0 = device_get_binding("GPIO_0");
  gpio_pin_configure(gpio0, 13, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_LOW);
  gpio_pin_configure(gpio0, 14, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_LOW);
  gpio_pin_configure(gpio0, 15, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_LOW);
  gpio_pin_configure(gpio0, 16, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_LOW);

  set_leds('\0');
}

static int led_settings_set(const char *name, size_t len,
                            settings_read_cb read_cb, void *cb_arg) {
  const char *next;
  int rc;

  if (settings_name_steq(name, "bits", &next) && !next) {
    if (len != sizeof(led_value)) {
      return -EINVAL;
    }

    rc = read_cb(cb_arg, &led_value, sizeof(led_value));
    if (rc >= 0) {
      set_leds(led_value);
      return 0;
    }

    return rc;
  }

  return -ENOENT;
}

struct settings_handler led_settings = {
    .name = "led",
    .h_set = led_settings_set,
};

void main(void) {
  init_gpio();

  settings_subsys_init();
  settings_register(&led_settings);
  settings_load();

  int ret = usb_enable(status_cb);
  if (ret != 0) {
    LOG_ERR("Failed to enable USB");
    return;
  }
}

static int pre_init(const struct device *dev) {
  hdev = device_get_binding("HID_0");

  if (hdev == NULL) {
    LOG_ERR("Cannot get USB HID Device");
    return -ENODEV;
  }

  LOG_INF("HID Device: dev %p", hdev);

  usb_hid_register_device(hdev, hid_report_desc, sizeof(hid_report_desc), &ops);

  atomic_set_bit(hid_ep_in_busy, HID_EP_BUSY_FLAG);

  if (usb_hid_set_proto_code(hdev, HID_BOOT_IFACE_CODE_NONE)) {
    LOG_WRN("Failed to set USB Protocol Code");
  }

  return usb_hid_init(hdev);
}

SYS_INIT(pre_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
