// Doritos Crash Course — game-specific import overrides.
#include <rex/hook.h>

#include "generated/default/doritoscrashcourse_init.h"

// Xbox Live Vision camera (xam XUsbcam*): the SDK does not implement them and a PC has no such
// device. Report "device not connected" (X_ERROR_DEVICE_NOT_CONNECTED = 0x48F), as Xenia does.
REX_STUB_RETURN(__imp__XUsbcamCreate, 0x48F)
REX_STUB_RETURN(__imp__XUsbcamDestroy, 0)
REX_STUB_RETURN(__imp__XUsbcamGetState, 0)  // XUSBCAM_STATE: 0 = not connected
REX_STUB_RETURN(__imp__XUsbcamReadFrame, 0x48F)
REX_STUB_RETURN(__imp__XUsbcamSetCaptureMode, 0x48F)
REX_STUB_RETURN(__imp__XUsbcamSetConfig, 0x48F)
REX_STUB_RETURN(__imp__XUsbcamSetView, 0x48F)
