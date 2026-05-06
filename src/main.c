#include <string.h>
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/resets.h"
#include "hardware/regs/usb.h"
#include "hardware/structs/usb.h"
#include "hardware/address_mapped.h"
#include "hardware/flash.h"

#include "usb_descs.h"

// ======================================================================
// USB Request Types (bmRequestType: setup_packet[0])
// ======================================================================
#define USB_REQ_TYPE_STANDARD       0x00 // GET_DESCRIPTOR 等の標準リクエスト
#define USB_REQ_TYPE_DIR_IN         0x80 // Device to Host
#define USB_REQ_TYPE_CLASS_INTF     0x21 // SET_IDLE 等の HID リクエスト

// ======================================================================
// USB Standard Requests (bRequest: setup_packet[1])
// ======================================================================
#define USB_REQ_GET_STATUS          0x00
#define USB_REQ_CLEAR_FEATURE       0x01
#define USB_REQ_SET_FEATURE         0x03
#define USB_REQ_SET_ADDRESS         0x05
#define USB_REQ_GET_DESCRIPTOR      0x06
#define USB_REQ_SET_DESCRIPTOR      0x07
#define USB_REQ_GET_CONFIGURATION   0x08
#define USB_REQ_SET_CONFIGURATION   0x09

// アトミック操作エイリアス
#define usb_hw_set   hw_set_alias(usb_hw)
#define usb_hw_clear hw_clear_alias(usb_hw)

typedef struct __attribute__((packed)) {
    int8_t   x;
    int8_t   y;
    uint16_t btn;
} hid_report_private_t;

typedef union {
    hid_report_private_t stru;
    uint32_t             raw32;
} hid_report_t;

uint8_t usb_mode_idx;
volatile uint32_t global_report_raw32 = 0;

static uint8_t dev_addr_pending = 0;
static bool ep1_in_toggle = false;
static bool ep1_out_toggle = false;

#define EP1_OUT_OFFSET  0x180
#define EP1_IN_OFFSET   0x1C0

#define FLASH_TARGET_OFFSET  (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define FLASH_TARGET_ADDRESS (XIP_BASE + FLASH_TARGET_OFFSET)

// ======================================================================
// HW 制御 util
// ======================================================================

// EP0 送信 (ACK付き)
static void ep0_send(const uint8_t *data, uint16_t len) {
    if (data && len > 0) memcpy((void*)usb_dpram->ep0_buf_a, data, len);
    usb_dpram->ep_buf_ctrl[0].in = len | USB_BUF_CTRL_AVAIL | USB_BUF_CTRL_FULL | USB_BUF_CTRL_DATA1_PID;
}

// EP0 ZLP 受信準備
static void ep0_receive_zlp(void) {
    usb_dpram->ep_buf_ctrl[0].out = 64 | USB_BUF_CTRL_AVAIL | USB_BUF_CTRL_DATA1_PID;
}

// EP0 STALL
static void ep0_stall(void) {
    usb_hw_set->ep_stall_arm = USB_EP_STALL_ARM_EP0_IN_BITS | USB_EP_STALL_ARM_EP0_OUT_BITS;
    usb_dpram->ep_buf_ctrl[0].in = USB_BUF_CTRL_STALL;
    usb_dpram->ep_buf_ctrl[0].out = USB_BUF_CTRL_STALL;
}

// EP1 IN 送信 (5 バイトのレポート)
static void ep1_in_send() {
    uint8_t *ep1_in_buf = (uint8_t*)usb_dpram + EP1_IN_OFFSET;

    uint32_t local_report_raw32 = global_report_raw32; // アトミックにコピー
    memcpy(ep1_in_buf, &local_report_raw32, 4); // ep1_in_buf にそのまま書く
    ep1_in_buf[4] = 0; // 5 バイト目は固定値の 0

    uint32_t pid = ep1_in_toggle ? USB_BUF_CTRL_DATA1_PID : USB_BUF_CTRL_DATA0_PID;
    ep1_in_toggle = !ep1_in_toggle;

    usb_dpram->ep_buf_ctrl[1].in = 5 | USB_BUF_CTRL_AVAIL | USB_BUF_CTRL_FULL | pid;
}

// EP1 OUT 受信待機
static void ep1_out_receive_prepare(void) {
    uint32_t pid = ep1_out_toggle ? USB_BUF_CTRL_DATA1_PID : USB_BUF_CTRL_DATA0_PID;
    usb_dpram->ep_buf_ctrl[1].out = 64 | USB_BUF_CTRL_AVAIL | pid;
}

// ======================================================================
// USB 割り込みハンドラ
// ======================================================================
void isr_usbctrl(void) {
    uint32_t status = usb_hw->ints;

    if (status & USB_INTS_BUS_RESET_BITS) {
        usb_hw->dev_addr_ctrl = 0;
        dev_addr_pending = 0;
        usb_hw_clear->sie_status = USB_SIE_STATUS_BUS_RESET_BITS;
    }

    if (status & USB_INTS_SETUP_REQ_BITS) {
        uint8_t req_type  = usb_dpram->setup_packet[0];
        uint8_t request   = usb_dpram->setup_packet[1];
        uint16_t value    = usb_dpram->setup_packet[2] | (usb_dpram->setup_packet[3] << 8);
        uint16_t length   = usb_dpram->setup_packet[6] | (usb_dpram->setup_packet[7] << 8);

        usb_hw_clear->sie_status = USB_SIE_STATUS_SETUP_REC_BITS;

        if (request == USB_REQ_GET_DESCRIPTOR) {
            uint8_t desc_type = value >> 8;
            uint8_t desc_idx  = value & 0xFF;
            const uint8_t *ptr = NULL;
            uint16_t len = 0;

            if (desc_type == DESC_TYPE_DEV) {
                ptr = usb_descs_arr[usb_mode_idx].dev.data;
                len = usb_descs_arr[usb_mode_idx].dev.len;
            } else if (desc_type == DESC_TYPE_CFG) {
                ptr = usb_descs_arr[usb_mode_idx].cfg.data;
                len = usb_descs_arr[usb_mode_idx].cfg.len;
            } else if (desc_type == DESC_TYPE_STR) {
                if (desc_idx < usb_descs_arr[usb_mode_idx].strings.len) {
                    ptr = usb_descs_arr[usb_mode_idx].strings.data[desc_idx];
                    len = ptr[0];
                }
            } else if (desc_type == DESC_TYPE_HID_REPORT) {
                ptr = usb_descs_arr[usb_mode_idx].report.data;
                len = usb_descs_arr[usb_mode_idx].report.len;
            }

            if (ptr) {
                uint16_t send_len = (length < len) ? length : len;
                ep0_send(ptr, send_len);
                ep0_receive_zlp();
            } else {
                ep0_stall();
            }
        } else if (request == USB_REQ_SET_ADDRESS) {
            dev_addr_pending = value & 0x7F;
            ep0_send(NULL, 0);
        } else if (request == USB_REQ_SET_CONFIGURATION) {
            // EP1 のハードウェア設定
            usb_dpram->ep_ctrl[0].out = EP_CTRL_ENABLE_BITS | EP_CTRL_INTERRUPT_PER_BUFFER | (3u << EP_CTRL_BUFFER_TYPE_LSB) | EP1_OUT_OFFSET;
            usb_dpram->ep_ctrl[0].in  = EP_CTRL_ENABLE_BITS | EP_CTRL_INTERRUPT_PER_BUFFER | (3u << EP_CTRL_BUFFER_TYPE_LSB) | EP1_IN_OFFSET;

            ep1_in_toggle = false;
            ep1_out_toggle = false;

            // バッファ管理開始。IN は初回分を埋めておき、OUT は待機開始
            ep1_in_send();
            ep1_out_receive_prepare();

            ep0_send(NULL, 0);
        } else if (req_type == USB_REQ_TYPE_CLASS_INTF) {
            ep0_send(NULL, 0);
        } else {
            ep0_stall();
        }
    }

    if (status & USB_INTS_BUFF_STATUS_BITS) {
        uint32_t buffers = usb_hw->buf_status;
        usb_hw_clear->buf_status = buffers;

        // Bit 0 = EP0 IN 完了
        if ((buffers & (1 << 0)) && dev_addr_pending) {
            usb_hw->dev_addr_ctrl = dev_addr_pending;
            dev_addr_pending = 0;
        }

        // Bit 2 = EP1 IN 完了
        if (buffers & (1 << 2)) {
            ep1_in_send();
        }

        // Bit 3 = EP1 OUT 完了
        if (buffers & (1 << 3)) {
            uint16_t len = usb_dpram->ep_buf_ctrl[1].out & USB_BUF_CTRL_LEN_MASK;
            uint8_t *ep1_out_buf = (uint8_t*)usb_dpram + EP1_OUT_OFFSET;

            uint8_t rx_buf[64];
            memcpy(rx_buf, ep1_out_buf, len);

            // トグル状態を更新してから次のOUT受信を待機
            ep1_out_toggle = !ep1_out_toggle;
            ep1_out_receive_prepare();
        }
    }
}

// ======================================================================
// メインループ
// ======================================================================
int main(void) {
    // GPIO init - all pins input + pullup
    for (uint i = 0; i < NUM_BANK0_GPIOS; ++i) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_pull_up(i);
    }

    sleep_ms(10);

    // GPIO の状態に応じて、フラッシュを書き換え
    uint8_t overwrite_mode = 0xff; // 0xff のままなら書き換えない
    if (!gpio_get(0)) overwrite_mode = 0;
    else if (!gpio_get(1)) overwrite_mode = 1;

    if (overwrite_mode != 0xff) {
        // ページ単位でフラッシュに書き込み
        uint8_t page_data[FLASH_PAGE_SIZE];
        memset(page_data, ~overwrite_mode, FLASH_PAGE_SIZE);
        flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
        flash_range_program(FLASH_TARGET_OFFSET, page_data, FLASH_PAGE_SIZE);
    }

    // 現在のフラッシュ保存値を読み出して、動作モード番号として使う
    // フラッシュの初期値が 0xff である都合で、ビット反転して解釈する
    const uint8_t* flash_data = (const uint8_t*)FLASH_TARGET_ADDRESS;
    usb_mode_idx = ~(flash_data[0]);

    int err = setup_usb_descs(&usb_mode_idx);
    if (err) return -1;

    reset_block(RESETS_RESET_USBCTRL_BITS);
    unreset_block_wait(RESETS_RESET_USBCTRL_BITS);
    memset((void*)usb_dpram, 0, sizeof(*usb_dpram));

    usb_hw->muxing = USB_USB_MUXING_TO_PHY_BITS | USB_USB_MUXING_SOFTCON_BITS;
    usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;
    usb_hw->main_ctrl = USB_MAIN_CTRL_CONTROLLER_EN_BITS;

    usb_hw->sie_ctrl = USB_SIE_CTRL_EP0_INT_1BUF_BITS | USB_SIE_CTRL_PULLUP_EN_BITS;
    usb_hw->inte = USB_INTS_BUFF_STATUS_BITS | USB_INTS_BUS_RESET_BITS | USB_INTS_SETUP_REQ_BITS;

    irq_set_exclusive_handler(USBCTRL_IRQ, isr_usbctrl);
    irq_set_enabled(USBCTRL_IRQ, true);

    while (true) {
        uint32_t gpioall = ~gpio_get_all();

        hid_report_t local_report;
        local_report.stru.x   = (int8_t)(gpioall & 0xff);
        local_report.stru.y   = (int8_t)((gpioall >> 8) & 0xff);
        local_report.stru.btn = (gpioall >> 16) & 0xffff;
        global_report_raw32 = local_report.raw32;
    }

    return 0;
}
