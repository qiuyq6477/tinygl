#include <iostream>
#include <SDL2/SDL.h>
#include <framework/application.h>
#include <third_party/glad/glad.h>

#ifdef _WIN32
    #include <direct.h>
    #define chdir _chdir
#else
    #include <unistd.h>
#endif

namespace framework {

Application::Application(const AppConfig& config) : m_config(config) {
}

Application::~Application() {
    cleanupSDL();
}

void Application::run() {
    initSDL();
    
    // Initialize user resources
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
            if (event.type == SDL_QUIT) {
                m_isRunning = false;
            }
            // Allow subclass to handle events (including UI input)
            onEvent(event);
        }

        // 3. Update
        onUpdate(dt);

        // 4. Render Frame (delegated to subclass)
        renderFrame();
        
        // Update Window Title with FPS
        static float timeSinceTitleUpdate = 0.0f;
        timeSinceTitleUpdate += dt;
        if (timeSinceTitleUpdate > 0.5f) {
            std::string title = m_config.title + " - FPS: " + std::to_string((int)fps);
            SDL_SetWindowTitle(m_window, title.c_str());
            timeSinceTitleUpdate = 0.0f;
        }
    }

    onDestroy();
}

void Application::quit() {
    m_isRunning = false;
}

void Application::initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        exit(1);
    }

    // Set working directory
    char *base_path = SDL_GetBasePath();
    if (base_path) {
        chdir(base_path);
        SDL_free(base_path);
    }

    // Create Window
    Uint32 flags = m_config.windowFlags;
    if (m_config.resizable) flags |= SDL_WINDOW_RESIZABLE;
    
    if (flags & SDL_WINDOW_OPENGL) {
        // Request OpenGL 4.1 Core
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        
        // Request Depth and Stencil buffers
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
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
}

void Application::cleanupSDL() {
    if (m_window) SDL_DestroyWindow(m_window);
    SDL_Quit();
}

} // namespace framework
