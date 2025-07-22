#include "http_response.h"

HttpResponse::HttpResponse(const std::string& root_dir)
    : root_(root_dir), file_(nullptr), file_len_(0), src_fd_(-1) {}

std::string HttpResponse::get_content_type(const std::string& suffix) {
    static std::unordered_map<std::string, std::string> type_map {
        {".html", "text/html"}, {".jpg", "image/jpeg"}, {".png", "image/png"},
        {".css", "text/css"}, {".js", "application/javascript"}
    };
    return type_map.count(suffix) ? type_map[suffix] : "text/plain";
}

bool HttpResponse::map_file(const std::string& full_path) {
    struct stat sb;
    if (stat(full_path.c_str(), &sb) < 0 || S_ISDIR(sb.st_mode)) return false;

    src_fd_ = open(full_path.c_str(), O_RDONLY);
    if (src_fd_ < 0) return false;

    file_len_ = sb.st_size;
    file_ = (const char*)mmap(0, file_len_, PROT_READ, MAP_PRIVATE, src_fd_, 0);
    return file_ != MAP_FAILED;
}

void HttpResponse::unmap_file() {
    if (file_) {
        munmap((void*)file_, file_len_);
        file_ = nullptr;
        close(src_fd_);
    }
}

void HttpResponse::make_response(const std::string& path, std::string& out_buf) {
    std::string full_path = root_ + path;
    if (path == "/") full_path = root_ + "/index.html";

    if (!map_file(full_path)) {
        map_file(root_ + "/404.html");
        out_buf = "HTTP/1.1 404 Not Found\r\n";
    } else {
        out_buf = "HTTP/1.1 200 OK\r\n";
    }

    std::string suffix = full_path.substr(full_path.find_last_of('.'));
    out_buf += "Content-Type: " + get_content_type(suffix) + "\r\n";
    out_buf += "Content-Length: " + std::to_string(file_len_) + "\r\n\r\n";
}
