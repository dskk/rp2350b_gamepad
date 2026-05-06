#include <stdint.h>

#ifndef __cplusplus
typedef uint16_t char16_t;
#endif

#define DEV_DESC_LEN         (18)
#define CFG_DESC_LEN         (9)
#define ITF_DESC_LEN         (9)
#define HID_DESC_LEN         (9)
#define EP_DESC_LEN          (7)

#define DESC_TYPE_DEV        (1)
#define DESC_TYPE_CFG        (2)
#define DESC_TYPE_STR        (3)
#define DESC_TYPE_ITF        (4)
#define DESC_TYPE_EP         (5)
#define DESC_TYPE_HID        (0x21)
#define DESC_TYPE_HID_REPORT (0x22)

#define DESC_LEN_DUMMY0      (0xa5)
#define DESC_LEN_DUMMY1      (0x5a)
#define STR_DESC_LANGID      ((const uint8_t[]){ 4, 3, 0x09, 0x04 })
#define SERIAL_STR           "Build " __DATE__ " " __TIME__

#define MAKE_SIZED_DESC(...) \
    { .len = sizeof((uint8_t[]){__VA_ARGS__}), \
      .data = (uint8_t[]){__VA_ARGS__} }

#define MAKE_STR_DESC(str) \
    (const uint8_t *)&(const struct __attribute__((packed)) { \
        uint8_t len; uint8_t type; char16_t chars[sizeof(u"" str) / 2 - 1]; \
    }){ sizeof(u"" str), 3, u"" str }

#define MAKE_STR_DESC_ARRAY(...) \
    { .len  = sizeof((const uint8_t* const[]){__VA_ARGS__}) / sizeof(const uint8_t*), \
      .data = (const uint8_t* const[]){__VA_ARGS__} }

// String Descriptor 配列に対応する構造体
typedef struct {
    uint8_t len;
    const uint8_t* const* data;
} str_desc_array_t;

// Device Descriptor などの各 Descriptor に対応する構造体
// サイズとデータを組にして持つ。MAKE_SIZED_DESC マクロで作成する
typedef struct {
    uint16_t len;
    uint8_t* data;
} sized_desc_t;

// 動作モードごとに、必要な Descriptor の情報をすべて束ねた構造体
typedef struct {
    const sized_desc_t dev;
    sized_desc_t cfg;
    const sized_desc_t report;
    const str_desc_array_t strings;
} usb_descs_t;

static usb_descs_t usb_descs_arr[] = {
    { // ==================== MODE 0: IIDX ====================
        #define _NUM_EP (2)
        .dev = MAKE_SIZED_DESC(
            DEV_DESC_LEN,                     // bLength
            DESC_TYPE_DEV,                    // bDescriptorType    Type 1 -> Device Descriptor
            0x00, 0x02,                       // bcdUSB             USB Version Number (BCD) = 0x0200 -> USB2.0
            // (Class, SubClass, Protocol) = (0x00, 0x00, 0x00) -> Use class information in the Interface Descriptors
            0x00,                             // bDeviceClass
            0x00,                             // bDeviceSubClass
            0x00,                             // bDeviceProtocol
            64,                               // bMaxPacketSize0    EP0 Packet Size = 64 Byte
            0xcf, 0x1c,                       // idVendor           VID = 0x1ccf
            0x48, 0x80,                       // idProduct          PID = 0x8048
            0x00, 0x01,                       // bcdDevice          Device Version Number (BCD) = 0x0100 -> V01.00
            0x01,                             // iManufacturer      Manufacturer String Descriptor = .strings[1]
            0x02,                             // iProduct           Product String Descriptor = .strings[2]
            0x03,                             // iSerialNumber      Serial Number String Descriptor = .strings[3]
            0x01                              // bNumConfigurations Number of Configuration = 1
        ),

        .cfg = MAKE_SIZED_DESC(
            // Configuration Descriptor
            CFG_DESC_LEN,                     // bLength
            DESC_TYPE_CFG,                    // bDescriptorType     Type 2 -> Configuration Descriptor
            DESC_LEN_DUMMY0, DESC_LEN_DUMMY1, // wTotalLength        Descriptor Length Including IF/EP Descriptor
            1,                                // bNumInterface       Number of Interface Descriptor = 1
            1,                                // bConfigurationValue Configuration Number = 1
            0,                                // iConfiguration      Configuration String Descriptor = None
            0x80,                             // bmAttributes        Bus Powered, No Remote Wakeup
            50,                               // bMaxPower           Max Current = 100 mA (50 * 2)
            ITF_DESC_LEN,                     // bLength
            DESC_TYPE_ITF,                    // bDescriptorType     Type 4 -> Interface Descriptor
            0,                                // bInterfaceNumber    Interface ID 0
            0,                                // bAlternateSetting   No Alternate Setting
            _NUM_EP,                          // bNumEndpoints       Number of Endpoints (excluding EP0)
            // (Class, SubClass, Protocol) = (0x03, 0x00, 0x00) -> HID Class, Not for Boot
            3,                                // bInterfaceClass
            0,                                // bInterfaceSubClass
            0,                                // bInterfaceProtocol
            0,                                // iInterface          Interface String Descriptor = None
            // HID Descriptor
            HID_DESC_LEN,                     // bLength
            DESC_TYPE_HID,                    // bDescriptorType     Type 0x21 -> HID Descriptor
            0x11, 0x01,                       // bcdHID              HID Version Number (BCD) = 0x0111 -> V1.11
            0,                                // bCountryCode        None
            1,                                // bNumDescriptors     Number of Dependent Descriptors  = 1
                                              // Info of Dependent Descriptors Follows
            DESC_TYPE_HID_REPORT,             // bDescriptorType     Type 0x22 -> Report Descriptor
            DESC_LEN_DUMMY0, DESC_LEN_DUMMY1, // wDescriptorLength   Report Descriptor Length
            // Endpoint Descriptor
            EP_DESC_LEN,                      // bLength
            DESC_TYPE_EP,                     // bDescriptorType     Type 5 -> Endpoint Descriptor
            0x01,                             // bEndPointAddress    EP1, OUT
            0x03,                             // bmAttributes        Interrupt Transfer
            64, 0,                            // bMaxPacketSize      Max Packet Size = 64 Byte
            1,                                // bInterval           Interval = 1 ms
            // Endpoint Descriptor
            EP_DESC_LEN,                      // bLength
            DESC_TYPE_EP,                     // bDescriptorType     Type 5 -> Endpoint Descriptor
            0x81,                             // bEndPointAddress    EP1, IN
            0x03,                             // bmAttributes        Interrupt Transfer
            64, 0,                            // bMaxPacketSize      Max Packet Size = 64 Byte
            1                                 // bInterval           Interval = 1 ms
        ),

        .report = MAKE_SIZED_DESC(
            0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
            0x09, 0x04,        // Usage (Joystick)
            0xA1, 0x01,        // Collection (Application)
            0x09, 0x01,        //   Usage (Pointer)
            0xA1, 0x00,        //   Collection (Physical)
            0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
            0x09, 0x30,        //     Usage (X)
            0x09, 0x31,        //     Usage (Y)
            0x15, 0x81,        //     Logical Minimum (-127)
            0x25, 0x7F,        //     Logical Maximum (127)
            0x75, 0x08,        //     Report Size (8)
            0x95, 0x02,        //     Report Count (2)
            0x81, 0x02,        //     Input (Data,Var,Abs)
            0x05, 0x09,        //     Usage Page (Button)
            0x19, 0x01,        //     Usage Minimum (0x01)
            0x29, 0x10,        //     Usage Maximum (0x10)
            0x15, 0x00,        //     Logical Minimum (0)
            0x25, 0x01,        //     Logical Maximum (1)
            0x75, 0x01,        //     Report Size (1)
            0x95, 0x10,        //     Report Count (16)
            0x81, 0x02,        //     Input (Data,Var,Abs)
            0x75, 0x08,        //     Report Size (8)
            0x95, 0x01,        //     Report Count (1)
            0x81, 0x03,        //     Input (Const,Var,Abs)
            0xC0,              //   End Collection
            0xC0               // End Collection
        ),

        .strings = MAKE_STR_DESC_ARRAY(
            STR_DESC_LANGID,                    // [0] Language ID
            MAKE_STR_DESC("Cialis"),            // [1] Manufacturer
            MAKE_STR_DESC("IIDX Controller"),   // [2] Product
            MAKE_STR_DESC(SERIAL_STR)           // [3] Serial
        )
        #undef _NUM_EP
    },
    { // ==================== MODE 1: SDVX ====================
        #define _NUM_EP (2)
        .dev = MAKE_SIZED_DESC(
            DEV_DESC_LEN,                     // bLength
            DESC_TYPE_DEV,                    // bDescriptorType    Type 1 -> Device Descriptor
            0x00, 0x02,                       // bcdUSB             USB Version Number (BCD) = 0x0200 -> USB2.0
            // (Class, SubClass, Protocol) = (0x00, 0x00, 0x00) -> Use class information in the Interface Descriptors
            0x00,                             // bDeviceClass
            0x00,                             // bDeviceSubClass
            0x00,                             // bDeviceProtocol
            64,                               // bMaxPacketSize0    EP0 Packet Size = 64 Byte
            0xcf, 0x1c,                       // idVendor           VID = 0x1ccf
            0x14, 0x10,                       // idProduct          PID = 0x1014
            0x00, 0x01,                       // bcdDevice          Device Version Number (BCD) = 0x0100 -> V01.00
            0x01,                             // iManufacturer      Manufacturer String Descriptor = .strings[1]
            0x02,                             // iProduct           Product String Descriptor = .strings[2]
            0x03,                             // iSerialNumber      Serial Number String Descriptor = .strings[3]
            0x01                              // bNumConfigurations Number of Configuration = 1
        ),

        .cfg = MAKE_SIZED_DESC(
            // Configuration Descriptor
            CFG_DESC_LEN,                     // bLength
            DESC_TYPE_CFG,                    // bDescriptorType     Type 2 -> Configuration Descriptor
            DESC_LEN_DUMMY0, DESC_LEN_DUMMY1, // wTotalLength        Descriptor Length Including IF/EP Descriptor
            1,                                // bNumInterface       Number of Interface Descriptor = 1
            1,                                // bConfigurationValue Configuration Number = 1
            0,                                // iConfiguration      Configuration String Descriptor = None
            0x80,                             // bmAttributes        Bus Powered, No Remote Wakeup
            50,                               // bMaxPower           Max Current = 100 mA (50 * 2)
            ITF_DESC_LEN,                     // bLength
            DESC_TYPE_ITF,                    // bDescriptorType     Type 4 -> Interface Descriptor
            0,                                // bInterfaceNumber    Interface ID 0
            0,                                // bAlternateSetting   No Alternate Setting
            _NUM_EP,                          // bNumEndpoints       Number of Endpoints (excluding EP0)
            // (Class, SubClass, Protocol) = (0x03, 0x00, 0x00) -> HID Class, Not for Boot
            3,                                // bInterfaceClass
            0,                                // bInterfaceSubClass
            0,                                // bInterfaceProtocol
            0,                                // iInterface          Interface String Descriptor = None
            // HID Descriptor
            HID_DESC_LEN,                     // bLength
            DESC_TYPE_HID,                    // bDescriptorType     Type 0x21 -> HID Descriptor
            0x11, 0x01,                       // bcdHID              HID Version Number (BCD) = 0x0111 -> V1.11
            0,                                // bCountryCode        None
            1,                                // bNumDescriptors     Number of Dependent Descriptors  = 1
                                              // Info of Dependent Descriptors Follows
            DESC_TYPE_HID_REPORT,             // bDescriptorType     Type 0x22 -> Report Descriptor
            DESC_LEN_DUMMY0, DESC_LEN_DUMMY1, // wDescriptorLength   Report Descriptor Length
            // Endpoint Descriptor
            EP_DESC_LEN,                      // bLength
            DESC_TYPE_EP,                     // bDescriptorType     Type 5 -> Endpoint Descriptor
            0x01,                             // bEndPointAddress    EP1, OUT
            0x03,                             // bmAttributes        Interrupt Transfer
            64, 0,                            // bMaxPacketSize      Max Packet Size = 64 Byte
            1,                                // bInterval           Interval = 1 ms
            // Endpoint Descriptor
            EP_DESC_LEN,                      // bLength
            DESC_TYPE_EP,                     // bDescriptorType     Type 5 -> Endpoint Descriptor
            0x81,                             // bEndPointAddress    EP1, IN
            0x03,                             // bmAttributes        Interrupt Transfer
            64, 0,                            // bMaxPacketSize      Max Packet Size = 64 Byte
            1                                 // bInterval           Interval = 1 ms
        ),

        .report = MAKE_SIZED_DESC(
            0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
            0x09, 0x04,        // Usage (Joystick)
            0xA1, 0x01,        // Collection (Application)
            0x09, 0x01,        //   Usage (Pointer)
            0xA1, 0x00,        //   Collection (Physical)
            0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
            0x09, 0x30,        //     Usage (X)
            0x09, 0x31,        //     Usage (Y)
            0x15, 0x81,        //     Logical Minimum (-127)
            0x25, 0x7F,        //     Logical Maximum (127)
            0x75, 0x08,        //     Report Size (8)
            0x95, 0x02,        //     Report Count (2)
            0x81, 0x02,        //     Input (Data,Var,Abs)
            0x05, 0x09,        //     Usage Page (Button)
            0x19, 0x01,        //     Usage Minimum (0x01)
            0x29, 0x10,        //     Usage Maximum (0x10)
            0x15, 0x00,        //     Logical Minimum (0)
            0x25, 0x01,        //     Logical Maximum (1)
            0x75, 0x01,        //     Report Size (1)
            0x95, 0x10,        //     Report Count (16)
            0x81, 0x02,        //     Input (Data,Var,Abs)
            0x75, 0x08,        //     Report Size (8)
            0x95, 0x01,        //     Report Count (1)
            0x81, 0x03,        //     Input (Const,Var,Abs)
            0xC0,              //   End Collection
            0xC0               // End Collection
        ),

        .strings = MAKE_STR_DESC_ARRAY(
            STR_DESC_LANGID,                    // [0] Language ID
            MAKE_STR_DESC("Cialis"),            // [1] Manufacturer
            MAKE_STR_DESC("SDVX Controller"),   // [2] Product
            MAKE_STR_DESC(SERIAL_STR)           // [3] Serial
        )
        #undef _NUM_EP
    },
};

// 手作業でディスクリプタをベタ書きするとミスが出やすいので、サイズを埋め込む箇所を自動的に処理する。
// main 関数の冒頭でこの関数を一度だけ呼び出すこと。引数には使いたいモード番号を与えること(そのようなモードがなければエラーになる)。
int setup_usb_descs(uint8_t* mode_idx) {
    int num_modes = sizeof(usb_descs_arr) / sizeof(usb_descs_arr[0]);

    // mode_idx が範囲外ならエラー
    if (*mode_idx >= num_modes) return -1;

    for (int i = 0; i < num_modes; ++i) {
        usb_descs_t* d = &usb_descs_arr[i];

        // Configuration Descriptor 全体のサイズが合っているか確認
        uint8_t num_ep = d->cfg.data[CFG_DESC_LEN + 4]; // bNumEndpoints の場所は ITF_DESC_BASE + 4
        if (d->cfg.len != CFG_DESC_LEN + ITF_DESC_LEN + HID_DESC_LEN + EP_DESC_LEN * num_ep) return -1;

        // Configuration Descriptor の wTotalLength を埋める。場所は先頭から 2 バイトオフセットしたところ
        // 書き換え前にダミー値が入っていることを確認
        if (d->cfg.data[2] != DESC_LEN_DUMMY0 || d->cfg.data[3] != DESC_LEN_DUMMY1) return -1;
        d->cfg.data[2] = d->cfg.len & 0xFF;
        d->cfg.data[3] = (d->cfg.len >> 8) & 0xFF;
        // HID Descriptor の wDescriptorLength を埋める。場所は HID_DESC_BASE + 7
        // 書き換え前にダミー値が入っていることを確認
        if (d->cfg.data[CFG_DESC_LEN + ITF_DESC_LEN + 7] != DESC_LEN_DUMMY0 ||
            d->cfg.data[CFG_DESC_LEN + ITF_DESC_LEN + 8] != DESC_LEN_DUMMY1) return -1;
        d->cfg.data[CFG_DESC_LEN + ITF_DESC_LEN + 7] = d->report.len & 0xFF;
        d->cfg.data[CFG_DESC_LEN + ITF_DESC_LEN + 8] = (d->report.len >> 8) & 0xFF;
    }

    return 0; // OK
}
