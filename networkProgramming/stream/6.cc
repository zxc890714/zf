class DiskUsageChecker {
public:
    struct DuResult {
        bool success;
        size_t usage;
        std::string error;
    };

    // 检查指定路径的磁盘使用情况
    static DuResult checkPath(const char* path) {
        DuResult result = {false, 0, ""};
        
        if (!path) {
            result.error = "Invalid path";
            return result;
        }

        char command[512] = {0};
        // -s: 只显示总计
        // -B1: 以字节为单位
        // 2>/dev/null: 重定向错误输出
        snprintf(command, sizeof(command), 
                "du -sB1 \"%s\" 2>/dev/null", 
                path);

        FILE* pipe = popen(command, "r");
        if (!pipe) {
            result.error = std::string("popen failed: ") + strerror(errno);
            return result;
        }

        char line[256] = {0};
        if (fgets(line, sizeof(line), pipe)) {
            char* endptr = nullptr;
            errno = 0;
            
            result.usage = strtoull(line, &endptr, 10);
            
            if (errno == ERANGE) {
                result.error = "Number out of range";
            } else if (endptr == line) {
                result.error = "No valid number found";
            } else {
                result.success = true;
            }
        }

        pclose(pipe);
        return result;
    }

    // 检查指定路径下某个深度的所有目录大小
    static std::vector<std::pair<std::string, size_t>> 
    checkPathWithDepth(const char* path, int depth = 1) {
        std::vector<std::pair<std::string, size_t>> results;
        
        char command[512] = {0};
        // --max-depth=N: 指定遍历深度
        snprintf(command, sizeof(command), 
                "du -B1 --max-depth=%d \"%s\" 2>/dev/null", 
                depth, path);

        FILE* pipe = popen(command, "r");
        if (!pipe) {
            return results;
        }

        char line[512] = {0};
        while (fgets(line, sizeof(line), pipe)) {
            size_t size;
            char path[256];
            // 解析du的输出格式："大小 路径"
            if (sscanf(line, "%zu %255[^\n]", &size, path) == 2) {
                results.emplace_back(path, size);
            }
        }

        pclose(pipe);
        return results;
    }
};

// 使用示例
void example_usage() {
    // 检查单个路径
    {
        auto result = DiskUsageChecker::checkPath("/path/to/check");
        if (result.success) {
            printf("Usage: %zu bytes (%.2f MB)\n", 
                   result.usage, 
                   (float)result.usage / (1024*1024));
        } else {
            printf("Check failed: %s\n", result.error.c_str());
        }
    }

    // 检查目录及其子目录
    {
        auto results = DiskUsageChecker::checkPathWithDepth("/path/to/check", 2);
        for (const auto& item : results) {
            printf("%s: %zu bytes\n", item.first.c_str(), item.second);
        }
    }
}

// 更实用的检查特定目录的例子
void check_specific_dirs() {
    const char* paths[] = {
        "/etc",
        "/var/log",
        "/home/user/downloads"
    };

    for (const auto& path : paths) {
        auto result = DiskUsageChecker::checkPath(path);
        if (result.success) {
            printf("%-20s: %8.2f MB\n", 
                   path, 
                   (float)result.usage / (1024*1024));
        }
    }
}
