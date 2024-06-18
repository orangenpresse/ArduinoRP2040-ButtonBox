/* Includes */
#include <Wire.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include "pio_encoder.h"
#include "Adafruit_TinyUSB.h"

/* Constants */

// I2C
#define SDA 0
#define SCL 1

// RP2040 included LED
#define LED LED_BUILTIN

// LCD Setup
#define SSD1315_ADDR 0x78
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// Define how many pages
#define PAGES 4

// Define how many triggers are needed for debouncing
#define DEBOUNCE 1

/* Pin Matrix GPIO Mapping */

// Row pins (will be set as output)
const int rows[] = {11, 12, 10};
// Column pins (will be set as input)
const int cols[] = {2, 3, 4, 5, 6, 7, 8, 9};

/* Global Variables */
 // Current button states 
int buttonStates[3][8];

// Rotary
PioEncoder e1(17);
PioEncoder e2(20);

#define BUTTON_LEFT_PIN 16
#define BUTTON_RIGHT_PIN 19
bool ButtonLeft = false;
bool ButtonRight = false;

int slider1 = 0;
int slider1_old = 0;
int slider2 = 0;
int slider2_old = 0;
int slider[PAGES][2];  // Current rot states

// Array to store labels for each page
// PAGES pages, 13 labels per page, 3 characters + null terminator
char labels[PAGES][13][4];

/* USB Descriptor */
#define HID_BUTTON_BOX(...) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     )                 ,\
  HID_USAGE      ( HID_USAGE_DESKTOP_JOYSTICK  )                 ,\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION )                 ,\
    /* Report ID if any */\
    __VA_ARGS__ \
    /* 8 bit X, Y (min -127, max 127) */ \
    HID_USAGE_PAGE     ( HID_USAGE_PAGE_DESKTOP                 ) ,\
    HID_USAGE          ( HID_USAGE_DESKTOP_X                    ) ,\
    HID_USAGE          ( HID_USAGE_DESKTOP_Y                    ) ,\
    HID_LOGICAL_MIN    ( 0x81                                   ) ,\
    HID_LOGICAL_MAX    ( 0x7F                                   ) ,\
    HID_REPORT_COUNT   ( 2                                      ) ,\
    HID_REPORT_SIZE    ( 8                                      ) ,\
    HID_INPUT          ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
    /* 20 Buttons */ \
    HID_USAGE_PAGE     ( HID_USAGE_PAGE_BUTTON                  ) ,\
    HID_USAGE_MIN      ( 1                                      ) ,\
    HID_USAGE_MAX      ( 20                                     ) ,\
    HID_LOGICAL_MIN    ( 0                                      ) ,\
    HID_LOGICAL_MAX    ( 1                                      ) ,\
    HID_REPORT_COUNT   ( 20                                     ) ,\
    HID_REPORT_SIZE    ( 1                                      ) ,\
    HID_INPUT          ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
    /* Padding to make the report size byte-aligned */ \
    HID_REPORT_COUNT   ( 12                                      ) ,\
    HID_REPORT_SIZE    ( 1                                      ) ,\
    HID_INPUT          ( HID_CONSTANT | HID_ARRAY | HID_ABSOLUTE) ,\
  HID_COLLECTION_END

// HID Report (must be equal PAGES)
uint8_t const desc_hid_report[] =
{
  HID_BUTTON_BOX(HID_REPORT_ID(1)),
  HID_BUTTON_BOX(HID_REPORT_ID(2)),
  HID_BUTTON_BOX(HID_REPORT_ID(3)),
  HID_BUTTON_BOX(HID_REPORT_ID(4))
};

// Report struct
typedef struct TU_ATTR_PACKED
{
  int8_t  x;
  int8_t  y;
  uint32_t buttons;
} hid_buttonbox_report_t;

// USB HID object registration
Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 2, false);

// Report for each gamepad
hid_buttonbox_report_t gp[PAGES];

// Current gamepad
int currentGamepad = 0;
