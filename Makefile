CXX = g++
CXXFLAGS = -std=c++14 -pthread -Wall
TARGET = tinywebserver

SRCS = main.cpp server/server.cpp threadpool/threadpool.cpp 
SRCS += http/http_request.cpp http/http_response.cpp timer/timer.cpp log/log.cpp
OBJS = $(SRCS:.cpp=.o)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

clean:
	rm -f $(TARGET) *.o server/*.o threadpool/*.o http/*.o timer/*.o log/*.o
