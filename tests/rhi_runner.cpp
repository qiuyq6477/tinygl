#include "rhi_runner_app.h"

int main(int argc, char** argv) {
    AppConfig config;
    config.title = "TinyGL RHI Runner";
    config.width = 1280;
    config.height = 720;
    config.resizable = false;
    config.windowFlags = SDL_WINDOW_OPENGL;
    RhiRunnerApp app(config);
    app.run();
    return 0;
}
