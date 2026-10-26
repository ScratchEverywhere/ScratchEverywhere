#include "input_nv2a.hpp"
#include <algorithm>
#include <blockExecutor.hpp>
#include <cstdint>
#include <cstring>
#include <input.hpp>
#include <log.hpp>
#include <math.hpp>
#include <renderer.hpp>
#include <runtime.hpp>

#ifdef __XBOX__
#include <hal/debug.h>
#include <usbh_lib.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <xid_driver.h>

static constexpr int CONTROLLER_DEADZONE_X = 10000;
static constexpr int CONTROLLER_DEADZONE_Y = 18000;
static constexpr int CONTROLLER_DEADZONE_TRIGGER = 30; // 0-255 range

static uint8_t s_padRawData[32] = {0};
static volatile bool s_hasPadData = false;
static xid_dev_t *s_activePadDev = nullptr;

static void on_xid_int_read(UTR_T *utr) {
    if (!utr || utr->status < 0) return;
    uint32_t len = utr->xfer_len < sizeof(s_padRawData) ? utr->xfer_len : sizeof(s_padRawData);
    if (utr->buff && len > 0) {
        memcpy(s_padRawData, utr->buff, len);
        s_hasPadData = true;
    }
    // Re-queue transfer for continuous polling
    utr->xfer_len = 0;
    utr->bIsTransferDone = 0;
    usbh_int_xfer(utr);
}

static void on_xid_connection(xid_dev_t *xid_dev, int status) {
    (void)status;
    if (xid_dev && xid_dev->xid_desc.bType == XID_TYPE_GAMECONTROLLER) {
        if (!s_activePadDev) {
            s_activePadDev = xid_dev;
            usbh_xid_read(s_activePadDev, 0, (void *)on_xid_int_read);
            Log::log("NV2A Input: Xbox Controller connected");
        }
    }
}

static void on_xid_disconnection(xid_dev_t *xid_dev, int status) {
    (void)status;
    if (xid_dev && xid_dev == s_activePadDev) {
        s_activePadDev = nullptr;
        s_hasPadData = false;
        Log::log("NV2A Input: Xbox Controller disconnected");
    }
}

static bool s_usbStackInitialized = false;

static void ensureUsbStack() {
    if (s_usbStackInitialized) return;
    usbh_core_init();
    usbh_xid_init();
    usbh_install_xid_conn_callback(on_xid_connection, on_xid_disconnection);
    s_usbStackInitialized = true;

    // Scan existing devices
    xid_dev_t *dev = usbh_xid_get_device_list();
    while (dev) {
        if (dev->xid_desc.bType == XID_TYPE_GAMECONTROLLER) {
            s_activePadDev = dev;
            usbh_xid_read(s_activePadDev, 0, (void *)on_xid_int_read);
            Log::log("NV2A Input: Initial Xbox Controller found");
            break;
        }
        dev = dev->next;
    }
}
#endif

InputDevice_NV2A::InputDevice_NV2A() {
#ifdef __XBOX__
    ensureUsbStack();
#endif
}

InputDevice_NV2A::~InputDevice_NV2A() = default;

std::array<int, 2> InputDevice_NV2A::getTouchPosition() {
    std::array<int, 2> pos = {Input::mousePointer.x, Input::mousePointer.y};
    return pos;
}

void InputDevice_NV2A::getInput() {
    Input::inputButtons.clear();
    Input::inputKeys.clear();
    Input::mousePointer.isPressed = false;

#ifdef __XBOX__
    usbh_pooling_hubs();

    if (!s_activePadDev) {
        // Retry locating connected gamepad
        xid_dev_t *dev = usbh_xid_get_device_list();
        while (dev) {
            if (dev->xid_desc.bType == XID_TYPE_GAMECONTROLLER) {
                s_activePadDev = dev;
                usbh_xid_read(s_activePadDev, 0, (void *)on_xid_int_read);
                break;
            }
            dev = dev->next;
        }
    }

    if (s_activePadDev && s_hasPadData) {
        uint8_t report[32];
        memcpy(report, s_padRawData, sizeof(report));

        // Check report length
        if (report[1] >= 14) {
            uint16_t dButtons = *reinterpret_cast<uint16_t *>(&report[2]);
            uint8_t btnA = report[4];
            uint8_t btnB = report[5];
            uint8_t btnX = report[6];
            uint8_t btnY = report[7];
            uint8_t btnBlack = report[8]; // shoulderR
            uint8_t btnWhite = report[9]; // shoulderL
            uint8_t triggerL = report[10];
            uint8_t triggerR = report[11];
            int16_t stickLX = *reinterpret_cast<int16_t *>(&report[12]);
            int16_t stickLY = *reinterpret_cast<int16_t *>(&report[14]);
            int16_t stickRX = *reinterpret_cast<int16_t *>(&report[16]);
            int16_t stickRY = *reinterpret_cast<int16_t *>(&report[18]);

            // D-Pad
            if (dButtons & (1 << 0)) {
                Input::buttonPress("dpadUp");
                if (btnWhite > 30) Input::mousePointer.y += 3;
            }
            if (dButtons & (1 << 1)) {
                Input::buttonPress("dpadDown");
                if (btnWhite > 30) Input::mousePointer.y -= 3;
            }
            if (dButtons & (1 << 2)) {
                Input::buttonPress("dpadLeft");
                if (btnWhite > 30) Input::mousePointer.x -= 3;
            }
            if (dButtons & (1 << 3)) {
                Input::buttonPress("dpadRight");
                if (btnWhite > 30) Input::mousePointer.x += 3;
            }

            // Start & Back
            if (dButtons & (1 << 4)) Input::buttonPress("start");
            if (dButtons & (1 << 5)) Input::buttonPress("back");

            // Thumb clicks
            if (dButtons & (1 << 6)) Input::buttonPress("LeftStickPressed");
            if (dButtons & (1 << 7)) Input::buttonPress("RightStickPressed");

            // Face Buttons
            if (btnA > 30) Input::buttonPress("A");
            if (btnB > 30) Input::buttonPress("B");
            if (btnX > 30) Input::buttonPress("X");
            if (btnY > 30) Input::buttonPress("Y");

            // Shoulders & Virtual Mouse
            if (btnWhite > 30) {
                Input::buttonPress("shoulderL");
                Input::mousePointer.isMoving = true;
            } else {
                Input::mousePointer.isMoving = false;
            }

            if (btnBlack > 30) {
                Input::buttonPress("shoulderR");
                if (btnWhite > 30 && Input::mousePointer.isMoving) {
                    Input::mousePointer.isPressed = true;
                }
            }

            // Analog Triggers
            if (triggerL > CONTROLLER_DEADZONE_TRIGGER) Input::buttonPress("LT");
            if (triggerR > CONTROLLER_DEADZONE_TRIGGER) Input::buttonPress("RT");

            // Left Thumbstick directions
            if (stickLX > CONTROLLER_DEADZONE_X) Input::buttonPress("LeftStickRight");
            if (stickLX < -CONTROLLER_DEADZONE_X) Input::buttonPress("LeftStickLeft");
            if (stickLY < -CONTROLLER_DEADZONE_Y) Input::buttonPress("LeftStickDown");
            if (stickLY > CONTROLLER_DEADZONE_Y) Input::buttonPress("LeftStickUp");

            // Right Thumbstick directions
            if (stickRX > CONTROLLER_DEADZONE_X) Input::buttonPress("RightStickRight");
            if (stickRX < -CONTROLLER_DEADZONE_X) Input::buttonPress("RightStickLeft");
            if (stickRY < -CONTROLLER_DEADZONE_Y) Input::buttonPress("RightStickDown");
            if (stickRY > CONTROLLER_DEADZONE_Y) Input::buttonPress("RightStickUp");

            // Normalized joystick values (-1.0f to 1.0f)
            Input::leftJoystick.first = static_cast<float>(stickLX) / 32767.0f;
            Input::leftJoystick.second = static_cast<float>(-stickLY) / 32767.0f; // invert Y for standard convention
            Input::rightJoystick.first = static_cast<float>(stickRX) / 32767.0f;
            Input::rightJoystick.second = static_cast<float>(-stickRY) / 32767.0f;

            // Move mouse pointer with analog stick when shoulderL is held
            if (btnWhite > 30) {
                if (std::abs(stickLX) > CONTROLLER_DEADZONE_X) {
                    Input::mousePointer.x += static_cast<int>(stickLX / 8000);
                }
                if (std::abs(stickLY) > CONTROLLER_DEADZONE_Y) {
                    Input::mousePointer.y += static_cast<int>(stickLY / 8000);
                }
            }
        }
    }
#endif

    if (!Input::inputKeys.empty()) {
        Input::inputKeys.push_back("any");
    }

    BlockExecutor::executeKeyHats();
    BlockExecutor::doSpriteClicking();
}

std::string InputDevice_NV2A::openSoftwareKeyboard(const char *hintText) {
    (void)hintText;
    return "";
}

void InputDevice_NV2A::setClipboardText(const std::string &text) {
    (void)text;
}
