#include "http_request.h"

void HttpRequest::reset() {
    state_ = ParseState::REQUEST_LINE;
    method_.clear(); path_.clear(); version_.clear();
    headers_.clear(); body_.clear();
}

void HttpRequest::parse(const std::string& raw_data) {
    std::istringstream stream(raw_data);
    std::string line;
    while (getline(stream, line) && state_ != ParseState::FINISH) {
        if (line.back() == '\r') line.pop_back();

        switch (state_) {
            case ParseState::REQUEST_LINE:
                parse_request_line(line);
                break;
            case ParseState::HEADERS:
                if (line.empty()) state_ = ParseState::FINISH;
                else parse_header(line);
                break;
            default:
                break;
        }
    }
}

void HttpRequest::parse_request_line(const std::string& line) {
    std::istringstream ss(line);
    ss >> method_ >> path_ >> version_;
    state_ = ParseState::HEADERS;
}

void HttpRequest::parse_header(const std::string& line) {
    auto pos = line.find(": ");
    if (pos != std::string::npos) {
        auto key = line.substr(0, pos);
        auto val = line.substr(pos + 2);
        headers_[key] = val;
    }
}
