#include "server/server.h"

int main() {
    MySQLConnectionPool::get_instance()->init(
    "localhost", 3306, "root", "9821", "yourdb", 10);

    Logger::instance().init(true,"log/tinywebserver");
    LOG_INFO("server starting...");
    Server server(8080,4);
    server.run();
    return 0;
}
