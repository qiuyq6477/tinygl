#include <third_party/glad/glad.h>
#include <iostream>
#include <SDL2/SDL.h>
#include <framework/application.h>
#include <framework/ui_renderer.h>
#include <rhi/gl_device.h>
#include <rhi/soft_device.h>
#include <rhi/encoder.h>

#ifdef _WIN32
    #include <direct.h>
    #define chdir _chdir
#else
    #include <unistd.h>
#endif

namespace framework {

Application::Application(const AppConfig& config) : m_config(config) {
    // 构造 Context
    // Always create soft context for now as it's used by UI and legacy tests
    // Even in GL mode, we might want to render UI to it or just keep it valid to prevent crashes
    m_context = std::make_unique<SoftRenderContext>(config.width, config.height);
}

Application::~Application() {
    cleanupSDL();
}

void Application::run() {
    initSDL();
    
    // 初始化 UI
    UIRenderer::init(&m_uiContext, m_graphicsDevice.get());

    // 初始化用户资源
    if (!onInit()) {
        std::cerr << "Application onInit failed. Exiting." << std::endl;
        return;
    }

    m_isRunning = true;
    m_lastTime = SDL_GetTicks64();

    // Frame timing variables
    const int FRAME_TIME_COUNT = 60;
    float frame_times[FRAME_TIME_COUNT] = {0};
    int frame_time_idx = 0;
    float fps = 0.0f;

    while (m_isRunning) {
        // 1. Time Calculation
        uint64_t currentTime = SDL_GetTicks64();
        float dt = (currentTime - m_lastTime) / 1000.0f;
        m_lastTime = currentTime;

        // FPS Calculation (Moving Average)
        frame_times[frame_time_idx] = dt;
        frame_time_idx = (frame_time_idx + 1) % FRAME_TIME_COUNT;
        float total_time = 0;
        for(int i=0; i<FRAME_TIME_COUNT; ++i) total_time += frame_times[i];
        if (total_time > 0) {
            fps = FRAME_TIME_COUNT / total_time;
        }

        // 2. Event Handling
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            UIRenderer::processInput(&m_uiContext, event);
            if (event.type == SDL_QUIT) {
                m_isRunning = false;
            }
            // 允许子类处理事件
            onEvent(event);
        }

        // 3. GUI Logic Preparation
        mu_begin(&m_uiContext);
        onGUI();
        mu_end(&m_uiContext);

        // 4. Update
        onUpdate(dt);

        // 5. Render (User submits draw calls to SoftRenderContext)
        onRender();

        // 6. UI Render (RHI)

        rhi::CommandEncoder encoder;
        UIRenderer::render(&m_uiContext, encoder, m_config.width, m_config.height);
        m_graphicsDevice->Submit(encoder.GetBuffer());

        if (m_config.backend == AppConfig::Backend::Software) {
            // 7. Present (Blit SoftRenderContext buffer to SDL Window)
            uint32_t* buffer = m_context->getColorBuffer();
            if (buffer) {
                // Update Texture
                SDL_UpdateTexture(
                    m_texture,
                    NULL,
                    buffer,
                    m_config.width * sizeof(uint32_t)
                );

                // Render Copy
                SDL_RenderClear(m_renderer);
                SDL_RenderCopy(m_renderer, m_texture, NULL, NULL);
                SDL_RenderPresent(m_renderer);
            }
        } else {
            // OpenGL Present
            SDL_GL_SwapWindow(m_window);
        }
        
        // Update Window Title with FPS (Optional, easier than drawing text for now)
        static float timeSinceTitleUpdate = 0.0f;
        timeSinceTitleUpdate += dt;
        if (timeSinceTitleUpdate > 0.5f) {
            std::string title = m_config.title + " - FPS: " + std::to_string((int)fps) + 
                                (m_config.backend == AppConfig::Backend::OpenGL ? " [GL]" : " [SW]");
            SDL_SetWindowTitle(m_window, title.c_str());
            timeSinceTitleUpdate = 0.0f;
        }
    }

    onDestroy();
    UIRenderer::shutdown();
}

void Application::quit() {
    m_isRunning = false;
}

void Application::initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        exit(1);
    }

    // 设置工作目录
    char *base_path = SDL_GetBasePath();
    if (base_path) {
        chdir(base_path);
        SDL_free(base_path);
    }

    // Create Window
    Uint32 flags = 0;
    if (m_config.resizable) flags |= SDL_WINDOW_RESIZABLE;
    
    if (m_config.backend == AppConfig::Backend::OpenGL) {
        flags |= SDL_WINDOW_OPENGL;
        // Request OpenGL 4.1 Core
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    }

    m_window = SDL_CreateWindow(
        m_config.title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        m_config.width, m_config.height,
        flags
    );

    if (!m_window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        exit(1);
    }

    if (m_config.backend == AppConfig::Backend::Software) {
        // Create Renderer
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
        if (!m_renderer) {
            std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
            exit(1);
        }

        // Create Texture (Streaming for frequent updates)
        m_texture = SDL_CreateTexture(
            m_renderer,
            SDL_PIXELFORMAT_RGBA32, // Match tinygl internal format
            SDL_TEXTUREACCESS_STREAMING,
            m_config.width, m_config.height
        );

        if (!m_texture) {
            std::cerr << "SDL_CreateTexture Error: " << SDL_GetError() << std::endl;
            exit(1);
        }
        
        // Create SoftDevice
        m_graphicsDevice = std::make_unique<rhi::SoftDevice>(*m_context);

    } else {
        // Initialize OpenGL
        m_glContext = SDL_GL_CreateContext(m_window);
        if (!m_glContext) {
            std::cerr << "SDL_GL_CreateContext Error: " << SDL_GetError() << std::endl;
            exit(1);
        }
        
        // Load GL function pointers
        if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
            std::cerr << "Failed to initialize GLAD" << std::endl;
            exit(1);
        }
        
        std::cout << "OpenGL Initialized: " << glGetString(GL_VERSION) << std::endl;
        
        // VSync
        SDL_GL_SetSwapInterval(1);

        // Create GL Device
        m_graphicsDevice = std::make_unique<rhi::GLDevice>();
    }
}

void Application::cleanupSDL() {
    if (m_texture) SDL_DestroyTexture(m_texture);
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_glContext) SDL_GL_DeleteContext(m_glContext);
    if (m_window) SDL_DestroyWindow(m_window);
    SDL_Quit();
}

} // namespace tinygl
