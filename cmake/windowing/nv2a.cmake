# Xbox (nxdk) NV2A direct windowing backend
if(XBOX)
    target_include_directories(windowing_interface INTERFACE
        ${NXDK_DIR}/lib/usb/libusbohci/inc
        ${NXDK_DIR}/lib/usb/libusbohci_xbox
    )
    target_compile_definitions(windowing_interface INTERFACE
        USBH_USE_EXTERNAL_CONFIG="usbh_config_xbox.h"
    )
endif()
