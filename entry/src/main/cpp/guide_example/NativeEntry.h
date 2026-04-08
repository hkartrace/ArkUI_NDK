#ifndef MYAPPLICATION_NATIVEENTRY_H
#define MYAPPLICATION_NATIVEENTRY_H

#include "ArkUIBaseNode.h"
#include <arkui/native_type.h>
#include <js_native_api_types.h>

namespace NativeModule {

napi_value CreateNativeRoot(napi_env env, napi_callback_info info);

napi_value DestroyNativeRoot(napi_env env, napi_callback_info info);

// Manage the lifecycle and memory of the native component.
class NativeEntry {
public:
    static NativeEntry *GetInstance() {
        static NativeEntry nativeEntry;
        return &nativeEntry;
    }

    void SetContentHandle(ArkUI_NodeContentHandle handle) {
        handle_ = handle;
    }

    void SetRootNode(const std::shared_ptr<ArkUIBaseNode> &baseNode) {
        root_ = baseNode;
        // Add the native component to NodeContent for mounting and display.
        OH_ArkUI_NodeContent_AddNode(handle_, root_->GetHandle());
    }
    void DisposeRootNode() {
        // Unmount the component from NodeContent and destroy the native component.
        OH_ArkUI_NodeContent_RemoveNode(handle_, root_->GetHandle());
        root_.reset();
    }

private:
    std::shared_ptr<ArkUIBaseNode> root_;
    ArkUI_NodeContentHandle handle_;
};

} // namespace NativeModule

#endif // MYAPPLICATION_NATIVEENTRY_H