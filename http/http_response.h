#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>
#include <unordered_map>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

class HttpResponse {
public:
    explicit HttpResponse(const std::string& root_dir);

    void make_response(const std::string& path, std::string& out_buf);
    const char* file_data() const { return file_; }
    size_t file_len() const { return file_len_; }

private:
    std::string root_;
    const char* file_;
    size_t file_len_;
    int src_fd_;

    std::string get_content_type(const std::string& suffix);
    bool map_file(const std::string& full_path);
    void unmap_file();
};

#endif
