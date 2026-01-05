#pragma once
#include <SDL2/SDL.h>
#include <iostream>
#include <string>
#include <tinygl/core/tinygl.h>
#include <tinygl/framework/ui_renderer.h>
#include <tinygl/third_party/microui.h>
namespace tinygl {

struct AppConfig {
    std::string title = "TinyGL Application";
    int width = 800;
    int height = 600;
    bool resizable = false;
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

    // 渲染：每帧调用。在此处提交绘制命令。
    virtual void onRender() {}

    // 事件：处理 SDL 事件。返回 true 表示事件已被消费。
    virtual void onEvent(const SDL_Event& event) {}

    // 销毁：在 run() 结束前调用。
    virtual void onDestroy() {}

    // --- 辅助函数 ---
    
    // 获取渲染上下文
    SoftRenderContext& getContext() { return *m_context; }
    const AppConfig& getConfig() const { return m_config; }

    // 获取 UI 上下文
    mu_Context* getUIContext() { return &m_uiContext; }

    // 获取窗口宽高
    int getWidth() const { return m_config.width; }
    int getHeight() const { return m_config.height; }

private:
    void initSDL();
    void cleanupSDL();
    void handleResize(int w, int h);

private:
    AppConfig m_config;
    bool m_isRunning = false;

    // SDL Resources
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture* m_texture = nullptr;

    // TinyGL Context
    std::unique_ptr<SoftRenderContext> m_context;
    
    // UI Context
    mu_Context m_uiContext;

    // Timing
    uint64_t m_lastTime = 0;
};

} // namespace tinygl
