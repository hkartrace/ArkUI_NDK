typedef int BOOL;
typedef unsigned long DWORD;
typedef void* HINSTANCE;
typedef void* LPVOID;
#define DLL_PROCESS_ATTACH 1
#define TRUE 1
#define WINAPI __stdcall

extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char*);

extern "C" void* memcpy(void* dest, const void* src, unsigned long n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    for (unsigned long i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

extern "C" void* memset(void* dest, int c, unsigned long n) {
    char* d = (char*)dest;
    for (unsigned long i = 0; i < n; i++) d[i] = (char)c;
    return dest;
}

typedef struct napi_env__* napi_env;
typedef struct napi_value__* napi_value;
typedef struct napi_callback_info__* napi_callback_info;

typedef enum {
    napi_ok,
    napi_invalid_arg,
    napi_object_expected,
    napi_string_expected,
    napi_name_expected,
    napi_function_expected,
    napi_number_expected,
    napi_boolean_expected,
    napi_array_expected,
    napi_generic_failure,
    napi_pending_exception,
    napi_cancelled,
    napi_escape_called_twice,
    napi_handle_scope_mismatch,
    napi_callback_scope_mismatch,
    napi_queue_full,
    napi_closing,
    napi_bigint_expected,
    napi_date_expected,
    napi_arraybuffer_expected,
    napi_detachable_arraybuffer_expected,
    napi_would_deadlock,
    napi_create_ark_runtime_too_many_envs = 22,
    napi_create_ark_runtime_only_one_env_per_thread = 23,
    napi_destroy_ark_runtime_env_not_exist = 24
} napi_status;

typedef enum {
    napi_default = 0,
    napi_writable = 1 << 0,
    napi_enumerable = 1 << 1,
    napi_configurable = 1 << 2,
    napi_static = 1 << 10,
    napi_default_method = napi_writable | napi_configurable,
    napi_default_jsproperty = napi_writable | napi_enumerable | napi_configurable,
} napi_property_attributes;

typedef napi_value (*napi_callback)(napi_env env, napi_callback_info info);

typedef struct {
    const char* utf8name;
    napi_value name;
    napi_callback method;
    napi_callback getter;
    napi_callback setter;
    napi_value value;
    napi_property_attributes attributes;
    void* data;
} napi_property_descriptor;

typedef napi_value (*napi_addon_register_func)(napi_env env, napi_value exports);

typedef struct napi_module {
    int nm_version;
    unsigned int nm_flags;
    const char* nm_filename;
    napi_addon_register_func nm_register_func;
    const char* nm_modname;
    void* nm_priv;
    void* reserved[4];
} napi_module;

extern "C" {
    __declspec(dllimport) void napi_module_register(napi_module* mod);
    __declspec(dllimport) napi_status napi_get_cb_info(
        napi_env env, napi_callback_info info, size_t* argc,
        napi_value* argv, napi_value* this_arg, void** data);
    __declspec(dllimport) napi_status napi_get_undefined(napi_env env, napi_value* result);
    __declspec(dllimport) napi_status napi_define_properties(
        napi_env env, napi_value object, size_t property_count,
        const napi_property_descriptor* properties);
}

static napi_value CreateNativeRoot(napi_env env, napi_callback_info info) {
    OutputDebugStringA("[libentry] CreateNativeRoot called!\n");

    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

static napi_value DestroyNativeRoot(napi_env env, napi_callback_info info) {
    OutputDebugStringA("[libentry] DestroyNativeRoot called!\n");
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

static napi_value Init(napi_env env, napi_value exports) {
    OutputDebugStringA("[libentry] Init called - registering properties!\n");

    napi_property_descriptor desc[] = {
        {"createNativeRoot", nullptr, CreateNativeRoot, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"destroyNativeRoot", nullptr, DestroyNativeRoot, nullptr, nullptr, nullptr, napi_default, nullptr}
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}

static napi_module demoModule = {
    1,
    0,
    nullptr,
    Init,
    "entry",
    nullptr,
    {nullptr, nullptr, nullptr, nullptr}
};

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        OutputDebugStringA("[libentry] DllMain - registering napi module 'entry'\n");
        napi_module_register(&demoModule);
    }
    return TRUE;
}
