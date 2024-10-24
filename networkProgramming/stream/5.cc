#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

class PartitionChecker {
public:
    struct Result {
        bool success;
        size_t usage;
        std::string error;
    };

    static Result check_usage(const char* mount_point, int timeout_seconds = 5) {
        Result result = {false, 0, ""};
        
        if (!mount_point) {
            result.error = "Invalid mount point";
            return result;
        }

        char command[256];
        snprintf(command, sizeof(command), 
                "timeout %d du -sB1 %s 2>/dev/null", 
                timeout_seconds, 
                mount_point);

        FILE* pipe = popen(command, "r");
        if (!pipe) {
            result.error = std::string("popen failed: ") + strerror(errno);
            return result;
        }

        char line[128];
        if (fgets(line, sizeof(line), pipe)) {
            char* endptr = nullptr;
            errno = 0;  // 重置errno
            
            result.usage = strtoull(line, &endptr, 10);
            
            if (errno == ERANGE) {
                result.error = "Number out of range";
            } else if (endptr == line) {
                result.error = "No valid number found";
            } else {
                result.success = true;
            }
        } else {
            result.error = "Failed to read du output";
        }

        // 检查命令执行状态
        int status = pclose(pipe);
        if (status == -1) {
            result.error = std::string("pclose failed: ") + strerror(errno);
            result.success = false;
        } else {
            if (WIFEXITED(status)) {
                int exit_status = WEXITSTATUS(status);
                if (exit_status == 124 || exit_status == 137) {  // timeout的退出码
                    result.error = "Command timed out";
                    result.success = false;
                } else if (exit_status != 0) {
                    result.error = "Command failed with status " + 
                                 std::to_string(exit_status);
                    result.success = false;
                }
            }
        }

        return result;
    }
};

// 使用示例
void check_partition(const char* mount_point) {
    auto result = PartitionChecker::check_usage(mount_point);
    
    if (result.success) {
        printf("Partition usage: %zu bytes (%.2f MB)\n", 
               result.usage, 
               (float)result.usage / (1024*1024));
    } else {
        printf("Check failed: %s\n", result.error.c_str());
    }
}

// 带重试机制的使用示例
bool check_partition_with_retry(const char* mount_point, 
                              size_t& usage,
                              int max_retries = 3) {
    for (int i = 0; i < max_retries; i++) {
        auto result = PartitionChecker::check_usage(mount_point);
        if (result.success) {
            usage = result.usage;
            return true;
        }
        
        // 失败后等待一段时间再重试
        if (i < max_retries - 1) {
            usleep(100000 * (i + 1));  // 递增等待时间
        }
    }
    return false;
}
