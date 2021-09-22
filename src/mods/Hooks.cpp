#include "Mods.hpp"
#include "REFramework.hpp"
#include "utility/Scan.hpp"
#include "utility/Module.hpp"

#include "sdk/Application.hpp"

#include "Hooks.hpp"

Hooks* g_hook = nullptr;

Hooks::Hooks() {
    g_hook = this;
}

std::optional<std::string> Hooks::on_initialize() {
    auto game = g_framework->get_module().as<HMODULE>();

    const auto mod_size = utility::get_module_size(game);

    if (!mod_size) {
        return "Unable to get module size";
    }

    for (auto hook : m_hook_list) {
        auto result = hook();

        // Error occurred when hooking
        if (result) {
            return result;
        }
    }

    return Mod::on_initialize();
}

std::optional<std::string> Hooks::hook_update_transform() {
    auto game = g_framework->get_module().as<HMODULE>();

    // The 48 8B 4D 40 bit might change.
    // Version 1.0 jmp stub: game+0x1dc7de0
    // Version 1
    //auto updateTransformCall = utility::scan(game, "E8 ? ? ? ? 48 8B 5B ? 48 85 DB 75 ? 48 8B 4D 40 48 31 E1");

    // Version 2 Dec 17th, 2019 (works on old version too) game.exe+0x1DD3FF0
    auto update_transform_call = utility::scan(game, "E8 ? ? ? ? 48 8B 5B ? 48 85 DB 75 ? 48 8B 4D 40 48 ? ?");

    if (!update_transform_call) {
        return "Unable to find UpdateTransform pattern.";
    }

    auto update_transform = utility::calculate_absolute(*update_transform_call + 1);
    spdlog::info("UpdateTransform: {:x}", update_transform);

    // Can be found by breakpointing RETransform's worldTransform
    m_update_transform_hook = std::make_unique<FunctionHook>(update_transform, &update_transform_hook);

    if (!m_update_transform_hook->create()) {
        return "Failed to hook UpdateTransform";
    }

    return std::nullopt;
}

std::optional<std::string> Hooks::hook_update_camera_controller() {
#if defined(RE2) || defined(RE3)
    // Version 1.0 jmp stub: game+0xB4685A0
    // Version 1
    /*auto updatecamera_controllerCall = utility::scan(game, "75 ? 48 89 FA 48 89 D9 E8 ? ? ? ? 48 8B 43 50 48 83 78 18 00 75 ? 45 89");

    if (!updatecamera_controllerCall) {
        return "Unable to find Updatecamera_controller pattern.";
    }

    auto updatecamera_controller = utility::calculate_absolute(*updatecamera_controllerCall + 9);*/

    // Version 2 Dec 17th, 2019 game.exe+0x7CF690 (works on old version too)
    //auto update_camera_controller = utility::scan(game, "40 55 56 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? 00 00 48 8B 41 50");

    // Version 3 June 2nd, 2020 game.exe+0xD41AD0 (works on old version too)
    auto update_camera_controller = sdk::find_native_method(game_namespace("camera.PlayerCameraController"), "updateCameraPosition");

    if (update_camera_controller == nullptr) {
        return std::string{"Failed to find "} + game_namespace("camera.PlayerCameraController") + "::updateCameraPosition";
    }

    spdlog::info("camera.PlayerCameraController.updateCameraPosition: {:x}", (uintptr_t)update_camera_controller);

    // Can be found by breakpointing camera controller's worldPosition
    m_update_camera_controller_hook = std::make_unique<FunctionHook>(update_camera_controller, &update_camera_controller_hook);

    if (!m_update_camera_controller_hook->create()) {
        return "Failed to hook UpdateCameraController";
    }
#endif

    return std::nullopt;
}

std::optional<std::string> Hooks::hook_update_camera_controller2() {
#if defined(RE2) || defined(RE3)
    // Version 1.0 jmp stub: game+0xCF2510
    // Version 1.0 function: game+0xB436230
    
    // Version 1
    //auto updatecamera_controller2 = utility::scan(game, "40 53 57 48 81 ec ? ? ? ? 48 8b 41 ? 48 89 d7 48 8b 92 ? ? 00 00");
    // Version 2 Dec 17th, 2019 game.exe+0x6CD9C0 (works on old version too)
    auto update_camera_controller2 = sdk::find_native_method(game_namespace("camera.TwirlerCameraControllerRoot"), "update");

    if (update_camera_controller2 == nullptr) {
        return std::string{"Failed to find "} + game_namespace("camera.TwirlerCameraControllerRoot") + "::update";
    }

    spdlog::info("camera.TwirlerCameraControllerRoot.update: {:x}", (uintptr_t)update_camera_controller2);

    // Can be found by breakpointing camera controller's worldRotation
    m_update_camera_controller2_hook = std::make_unique<FunctionHook>(update_camera_controller2, &update_camera_controller2_hook);

    if (!m_update_camera_controller2_hook->create()) {
        return "Failed to hook Updatecamera_controller2";
    }
#endif

    return std::nullopt;
}

std::optional<std::string> Hooks::hook_gui_draw() {
    auto game = g_framework->get_module().as<HMODULE>();
    auto application = sdk::Application::get();

    // This pattern appears to work all the way from RE2 to RE8.
    // If this ever breaks, its parent function is found within via.gui.GUIManager.
    // It is used as a draw callback. The assignment can be found within the constructor near the end.
    // "onEnd(via.gui.TextAnimationEndArg)" can be used as a reference to find the constructor.
    // In RE2:
    /*  
    *(_QWORD *)(v23 + 8 * v22) = &vtable_thing;
    *(_QWORD *)(v23 + 8 * v22 + 8) = gui_manager;
    *(_OWORD *)(v23 + 8 * v22 + 16) = v34;
    *(_QWORD *)(v23 + 8 * v22 + 32) = gui_manager;
    ++*(_DWORD *)(gui_manager + 232);
    *(_QWORD *)&v35 = draw_task_function; <-- "gui_draw_call" is found within this function.
    */
    auto gui_draw_call = utility::scan(game, "49 8B 0C CE 48 83 79 10 00 74 ? E8 ? ? ? ?");

    if (!gui_draw_call) {
        return "Unable to find gui_draw_call pattern.";
    }

    auto gui_draw = utility::calculate_absolute(*gui_draw_call + 12);
    spdlog::info("gui_draw_call: {:x}", gui_draw);

    m_gui_draw_hook = std::make_unique<FunctionHook>(gui_draw, &gui_draw_hook);

    if (!m_gui_draw_hook->create()) {
        return "Failed to hook GUI::draw";
    }

    return std::nullopt;
}

std::optional<std::string> Hooks::hook_application_entry(std::string name, std::unique_ptr<FunctionHook>& hook, void (*hook_fn)(void*)) {
    auto application = sdk::Application::get();

    if (application == nullptr) {
        return "Failed to get via.Application";
    }

    auto entry = application->get_function(name);

    if (entry == nullptr) {
        return "Unable to find via::Application::" + name;
    }

    auto func = entry->func;

    if (func == nullptr) {
        return "via::Application::" + name + " is null";
    }

    spdlog::info("{} entry: {:x}", name, (uintptr_t)entry);
    spdlog::info("{}: {:x}", name, (uintptr_t)func - g_framework->get_module());

    hook = std::make_unique<FunctionHook>(func, hook_fn);

    if (!hook->create()) {
        return "Failed to hook via::Application::" + name;
    }
    
    spdlog::info("Hooked via::Application::{}", name);

    return std::nullopt;
}

std::optional<std::string> Hooks::hook_update_before_lock_scene() {
    // Hook updateBeforeLockScene
    auto update_before_lock_scene = sdk::find_native_method("via.render.EntityRenderer", "updateBeforeLockScene");

    if (update_before_lock_scene == nullptr) {
        return "Unable to find via::render::EntityRenderer::updateBeforeLockScene";
    }

    spdlog::info("updateBeforeLockScene: {:x}", (uintptr_t)update_before_lock_scene);

    m_update_before_lock_scene_hook = std::make_unique<FunctionHook>(update_before_lock_scene, &update_before_lock_scene_hook);

    if (!m_update_before_lock_scene_hook->create()) {
        return "Failed to hook via::render::EntityRenderer::updateBeforeLockScene";
    }

    return std::nullopt;
}

void* Hooks::update_transform_hook_internal(RETransform* t, uint8_t a2, uint32_t a3) {
    if (!g_framework->is_ready()) {
        return m_update_transform_hook->get_original<decltype(update_transform_hook)>()(t, a2, a3);
    }

    auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_pre_update_transform(t);
    }

    auto ret = m_update_transform_hook->get_original<decltype(update_transform_hook)>()(t, a2, a3);

    for (auto& mod : mods) {
        mod->on_update_transform(t);
    }

    return ret;
}

void* Hooks::update_transform_hook(RETransform* t, uint8_t a2, uint32_t a3) {
    return g_hook->update_transform_hook_internal(t, a2, a3);
}

void* Hooks::update_camera_controller_hook_internal(void* a1, RopewayPlayerCameraController* camera_controller) {
    if (!g_framework->is_ready()) {
        return m_update_camera_controller_hook->get_original<decltype(update_camera_controller_hook)>()(a1, camera_controller);
    }

    auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_pre_update_camera_controller(camera_controller);
    }

    auto ret = m_update_camera_controller_hook->get_original<decltype(update_camera_controller_hook)>()(a1, camera_controller);

    for (auto& mod : mods) {
        mod->on_update_camera_controller(camera_controller);
    }

    return ret;
}

void* Hooks::update_camera_controller_hook(void* a1, RopewayPlayerCameraController* camera_controller) {
    return g_hook->update_camera_controller_hook_internal(a1, camera_controller);
}

void* Hooks::update_camera_controller2_hook_internal(void* a1, RopewayPlayerCameraController* camera_controller) {
    if (!g_framework->is_ready()) {
        return m_update_camera_controller2_hook->get_original<decltype(update_camera_controller2_hook)>()(a1, camera_controller);
    }

    auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_pre_update_camera_controller2(camera_controller);
    }

    auto ret = m_update_camera_controller2_hook->get_original<decltype(update_camera_controller2_hook)>()(a1, camera_controller);

    for (auto& mod : mods) {
        mod->on_update_camera_controller2(camera_controller);
    }

    return ret;
}

void* Hooks::update_camera_controller2_hook(void* a1, RopewayPlayerCameraController* camera_controller) {
    return g_hook->update_camera_controller2_hook_internal(a1, camera_controller);
}

void* Hooks::gui_draw_hook_internal(REComponent* gui_element, void* primitive_context) {
    auto original_func = m_gui_draw_hook->get_original<decltype(gui_draw_hook)>();

    if (!g_framework->is_ready()) {
        return original_func(gui_element, primitive_context);
    }

    auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_pre_gui_draw_element(gui_element, primitive_context);
    }

    auto ret = original_func(gui_element, primitive_context);

    for (auto& mod : mods) {
        mod->on_gui_draw_element(gui_element, primitive_context);
    }

    return ret;
}

void* Hooks::gui_draw_hook(REComponent* gui_element, void* primitive_context) {
    return g_hook->gui_draw_hook_internal(gui_element, primitive_context);
}

void Hooks::update_before_lock_scene_hook_internal(void* ctx) {
    auto original = m_update_before_lock_scene_hook->get_original<decltype(update_before_lock_scene_hook)>();

    if (!g_framework->is_ready()) {
        return original(ctx);
    }

    auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_pre_update_before_lock_scene(ctx);
    }

    original(ctx);

    for (auto& mod : mods) {
        mod->on_update_before_lock_scene(ctx);
    }
}

void Hooks::update_before_lock_scene_hook(void* ctx) {
    g_hook->update_before_lock_scene_hook_internal(ctx);
}

void Hooks::lock_scene_hook_internal(void* entry) {
    auto original = m_lock_scene_hook->get_original<decltype(lock_scene_hook)>();

    if (!g_framework->is_ready()) {
        return original(entry);
    }

    auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_pre_lock_scene(entry);
    }

    original(entry);

    for (auto& mod : mods) {
        mod->on_lock_scene(entry);
    }
}

void Hooks::lock_scene_hook(void* entry) {
    g_hook->lock_scene_hook_internal(entry);
}

void Hooks::begin_rendering_hook_internal(void* entry) {
    auto original = m_begin_rendering_hook->get_original<decltype(begin_rendering_hook)>();

    if (!g_framework->is_ready()) {
        return original(entry);
    }

    auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_pre_begin_rendering(entry);
    }

    original(entry);

    for (auto& mod : mods) {
        mod->on_begin_rendering(entry);
    }
}

void Hooks::begin_rendering_hook(void* entry) {
    g_hook->begin_rendering_hook_internal(entry);
}

void Hooks::end_rendering_hook_internal(void* entry) {
    auto original = m_end_rendering_hook->get_original<decltype(end_rendering_hook)>();

    if (!g_framework->is_ready()) {
        return original(entry);
    }

    auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_pre_end_rendering(entry);
    }

    original(entry);

    for (auto& mod : mods) {
        mod->on_end_rendering(entry);
    }
}

void Hooks::end_rendering_hook(void* entry) {
    g_hook->end_rendering_hook_internal(entry);
}

void Hooks::wait_rendering_hook_internal(void* entry) {
    auto original = m_wait_rendering_hook->get_original<decltype(wait_rendering_hook)>();

    if (!g_framework->is_ready()) {
        return original(entry);
    }

    auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_pre_wait_rendering(entry);
    }

    original(entry);

    for (auto& mod : mods) {
        mod->on_wait_rendering(entry);
    }
}

void Hooks::wait_rendering_hook(void* entry) {
    g_hook->wait_rendering_hook_internal(entry);
}
