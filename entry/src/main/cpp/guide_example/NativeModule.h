// NativeModule.h
// Provide encapsulated APIs for obtaining ArkUI modules on the native side.

#ifndef MYAPPLICATION_NATIVEMODULE_H
#define MYAPPLICATION_NATIVEMODULE_H

#include "napi/native_api.h"
#include <arkui/native_node.h>
#include <cassert>

#include <arkui/native_interface.h>

namespace NativeModule {

class NativeModuleInstance {
public:
    static NativeModuleInstance *GetInstance() {
        static NativeModuleInstance instance;
        return &instance;
    }

    NativeModuleInstance() {
        // Obtain the function pointer struct of the NDK API for subsequent operations.
        OH_ArkUI_GetModuleInterface(ARKUI_NATIVE_NODE, ArkUI_NativeNodeAPI_1, arkUINativeNodeApi_);
        assert(arkUINativeNodeApi_);
    }
    // Expose it for use by other modules.
    ArkUI_NativeNodeAPI_1 *GetNativeNodeAPI() { return arkUINativeNodeApi_; }

private:
    ArkUI_NativeNodeAPI_1 *arkUINativeNodeApi_ = nullptr;
};

} // namespace NativeModule

#endif // MYAPPLICATION_NATIVEMODULE_H