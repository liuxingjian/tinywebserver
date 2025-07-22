#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H
#include <string>
#include <unordered_map>
#include <sstream>
#include <iostream>
class HttpRequest {
public:
    enum class ParseState { REQUEST_LINE, HEADERS, BODY, FINISH };
    HttpRequest(){reset();} 
    void parse(const std::string& raw_data);
    std::string get_method() const { return method_; }
    std::string get_path() const {return path_;}
private:
    ParseState state_;
    std::string method_, path_, version_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
    void parse_request_line(const std::string& line);
    void parse_header(const std::string& line);
    void reset();
};




#endif
