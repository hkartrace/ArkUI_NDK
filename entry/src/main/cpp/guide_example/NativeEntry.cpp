// NativeEntry.cpp

#include <arkui/native_node_napi.h>
#include <hilog/log.h>
#include <js_native_api.h>
#include "NativeEntry.h"
#include "NormalTextListExample.h"

namespace NativeModule {

napi_value CreateNativeRoot(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};

    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    // Obtain NodeContent.
    ArkUI_NodeContentHandle contentHandle;
    OH_ArkUI_GetNodeContentFromNapiValue(env, args[0], &contentHandle);
    NativeEntry::GetInstance()->SetContentHandle(contentHandle);

    // Create a text list.
    auto list = CreateTextListExample();

    // Keep the native side object in the management class to maintain its lifecycle.
    NativeEntry::GetInstance()->SetRootNode(list);
    return nullptr;
}

napi_value DestroyNativeRoot(napi_env env, napi_callback_info info) {
    // Release the native side object from the management class.
    NativeEntry::GetInstance()->DisposeRootNode();
    return nullptr;
}

} // namespace NativeModule