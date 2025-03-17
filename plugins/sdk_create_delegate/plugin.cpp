#include "reframework/API.hpp"
#include "sol/sol.hpp"

#include <array>
#include <cstdint>
#include <exception>
#include <format>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

extern "C" __declspec(dllexport) void reframework_plugin_required_version(REFrameworkPluginVersion* version) {
    version->major = REFRAMEWORK_PLUGIN_VERSION_MAJOR;
    version->minor = REFRAMEWORK_PLUGIN_VERSION_MINOR;
    version->patch = REFRAMEWORK_PLUGIN_VERSION_PATCH;
}

struct Invocation {
    void* object_ptr;
    void* func_ptr;
    void* unk;
};

class REObject {
public:
    class REObjectInfo* info;
};

class REManagedObjectD : public REObject {
public:
    uint32_t refcount; // 0x0008
    int16_t unkn;
    char pad[2];
};

struct Delegate : REManagedObjectD {
    uint32_t count{1};
    Invocation invocation;
};

struct ByteArray : REManagedObjectD {
    uint32_t count;
    uint8_t data[];

    template <typename T> T* as() { return reinterpret_cast<T*>(data); }
};

struct LuaFunc {
    sol::protected_function func;
};

template <class... Args> void call_lua(void* ctx, ByteArray* object_ptr, Args... args) {
    reframework::API::get()->log_warn("CALLED 0x%x", object_ptr);
    object_ptr->as<LuaFunc>()->func(args...);
}

void load_lua_state(lua_State* state) {
    sol::state_view s{state};

    auto api = &*reframework::API::get();
    auto byte_type = api->tdb()->find_type("System.Byte");
    auto action_type = api->tdb()->find_type("System.Action");
    auto sdk = s["sdk"];
    sdk["create_action"] = [=](sol::this_state state, int argument_count, sol::protected_function func) {
        auto dummy = api->create_managed_array(byte_type, sizeof(LuaFunc));
        dummy->add_ref();
        auto array = (ByteArray*)dummy;
        new (array->as<LuaFunc>()) LuaFunc{func};

        auto obj = action_type->create_instance();
        obj->add_ref();
        auto delegate = (Delegate*)obj;
        delegate->invocation.object_ptr = dummy;
        switch (argument_count) {
        default:
        case 0:
            delegate->invocation.func_ptr = (void*)call_lua<>;
            break;
        case 1:
            delegate->invocation.func_ptr = (void*)call_lua<void*>;
            break;
        case 2:
            delegate->invocation.func_ptr = (void*)call_lua<void*, void*>;
            break;
        case 3:
            delegate->invocation.func_ptr = (void*)call_lua<void*, void*, void*>;
            break;
        case 4:
            delegate->invocation.func_ptr = (void*)call_lua<void*, void*, void*, void*>;
            break;
        case 5:
            delegate->invocation.func_ptr = (void*)call_lua<void*, void*, void*, void*, void*>;
            break;
        case 6:
            delegate->invocation.func_ptr = (void*)call_lua<void*, void*, void*, void*, void*, void*>;
            break;
        }
        reframework::API::get()->log_warn("Created action 0x%x", delegate);
        return (void*)delegate;
    };
}

extern "C" __declspec(dllexport) bool reframework_plugin_initialize(const REFrameworkPluginInitializeParam* param) {
    reframework::API::initialize(param);
    param->functions->on_lua_state_created(load_lua_state);
    return true;
}
