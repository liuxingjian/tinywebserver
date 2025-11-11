#include <unistd.h>
#include "server/webserver.h"


int main(){
    WebServer server(
        9006,3,60000,false,
        3306,"root","root","webdb",
        12,6,10000,true,1,1024
    );
    server.start();
}