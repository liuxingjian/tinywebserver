#include "server/server.h"

int main() {
    Logger::instance().init(true,"log/tinywebserver");
    LOG_INFO("server starting...");
    Server server(8080,4);
    server.run();
    return 0;
}
