#pragma once
#include <SDL2/SDL.h>
#include <iostream>
#include <string>
#include <tinygl/tinygl.h>
#include <framework/ui_renderer.h>
#include <microui.h>
#include <rhi/device.h>

namespace framework {

struct AppConfig {
    std::string title = "TinyGL Application";
    int width = 800;
    int height = 600;
    bool resizable = false;
    uint32_t windowFlags = 0;
};

class TINYGL_API Application {
public:
    Application(const AppConfig& config);
    virtual ~Application();

    // 禁止拷贝和移动（因为持有 SDL 窗口句柄等唯一资源）
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // 运行应用程序（阻塞直到退出）
    void run();

    // 退出应用程序
    void quit();

protected:
    // --- 生命周期回调 (Lifecycle Callbacks) ---

    // 初始化：在 run() 开始时调用。返回 false 则退出。
    virtual bool onInit() { return true; }

    // 更新：每帧调用。dt 为上一帧到现在的秒数。
    virtual void onUpdate(float dt) {}

    // GUI 绘制：每帧调用。在此处调用 microui 函数。
    virtual void onGUI() {}

    // 核心渲染帧：由子类实现具体的渲染逻辑（包括场景和UI）
    virtual void renderFrame() = 0;

    // 事件：处理 SDL 事件。返回 true 表示事件已被消费。
    virtual void onEvent(const SDL_Event& event) {}

    // 销毁：在 run() 结束前调用。
    virtual void onDestroy() {}

    // --- 辅助函数 ---
    
    const AppConfig& getConfig() const { return m_config; }

    // 获取窗口宽高
    int getWidth() const { return m_config.width; }
    int getHeight() const { return m_config.height; }

    // 获取 SDL 窗口 (供子类使用，例如 SwapBuffer)
    SDL_Window* getWindow() { return m_window; }

protected:
    void initSDL();
    void cleanupSDL();

    AppConfig m_config;
    bool m_isRunning = false;

    // SDL Resources
    SDL_Window* m_window = nullptr;

    // Timing
    uint64_t m_lastTime = 0;
};

} // namespace tinygl
