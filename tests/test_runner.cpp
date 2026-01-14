#include "test_runner_app.h"

int main(int argc, char** argv) {
    AppConfig config;
    config.title = "TinyGL Test Runner";
    config.width = 1280;
    config.height = 720;
    config.resizable = true;
    config.windowFlags = 0; // Pure Software Mode

    TestRunnerApp app(config);
    app.run();
    return 0;
}
