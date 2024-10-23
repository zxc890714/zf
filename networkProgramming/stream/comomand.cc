#include <cstdlib>
#include <mtd/mtd-user.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

class MTDMonitor {
    // ... (与之前相同的MTDStatus结构和其他方法) ...
};

void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -d, --device <device>    Specify MTD device (e.g., /dev/mtd0)\n");
    printf("  -t, --threshold <value>  Set available space threshold (default: 20%%)\n");
    printf("  -m, --monitor            Enable periodic monitoring\n");
    printf("  -i, --interval <seconds> Set monitoring interval (default: 3600s)\n");
    printf("  -f, --force-format      Force format without confirmation\n");
    printf("  -h, --help              Show this help message\n");
}

struct ProgramOptions {
    char device[64];
    float threshold;
    bool monitor;
    int interval;
    bool force_format;
    
    ProgramOptions() {
        memset(device, 0, sizeof(device));
        threshold = 20.0;
        monitor = false;
        interval = 3600;
        force_format = false;
    }
};

int main(int argc, char* argv[]) {
    ProgramOptions opts;
    
    // 定义长选项
    static struct option long_options[] = {
        {"device",    required_argument, 0, 'd'},
        {"threshold", required_argument, 0, 't'},
        {"monitor",   no_argument,       0, 'm'},
        {"interval",  required_argument, 0, 'i'},
        {"force-format", no_argument,    0, 'f'},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;

    // 解析命令行参数
    while ((c = getopt_long(argc, argv, "d:t:mi:fh", 
                           long_options, &option_index)) != -1) {
        switch (c) {
            case 'd':
                strncpy(opts.device, optarg, sizeof(opts.device)-1);
                break;
            case 't':
                opts.threshold = atof(optarg);
                break;
            case 'm':
                opts.monitor = true;
                break;
            case 'i':
                opts.interval = atoi(optarg);
                break;
            case 'f':
                opts.force_format = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case '?':
                print_usage(argv[0]);
                return 1;
        }
    }

    // 验证必要参数
    if (strlen(opts.device) == 0) {
        printf("Error: MTD device must be specified\n");
        print_usage(argv[0]);
        return 1;
    }

    // 验证参数合理性
    if (opts.threshold <= 0 || opts.threshold >= 100) {
        printf("Error: Threshold must be between 0 and 100\n");
        return 1;
    }

    if (opts.interval < 1) {
        printf("Error: Invalid monitoring interval\n");
        return 1;
    }

    // 执行MTD监控
    if (opts.monitor) {
        printf("Starting MTD monitor for %s\n", opts.device);
        printf("Threshold: %.1f%%, Interval: %d seconds\n", 
               opts.threshold, opts.interval);
        
        while(true) {
            MTDMonitor::MTDStatus status = 
                MTDMonitor::check_partition_status(opts.device, opts.threshold);
            
            printf("\nMTD Status Check at %s", ctime(&(time_t){time(NULL)}));
            printf("Available: %.1f%%\n", status.available_percent);
            
            if (status.needs_format) {
                printf("Warning: Available space below threshold!\n");
                if (opts.force_format) {
                    printf("Auto-formatting partition...\n");
                    if (MTDMonitor::format_partition(opts.device)) {
                        printf("Format successful\n");
                    } else {
                        printf("Format failed\n");
                    }
                } else {
                    printf("Manual intervention required\n");
                }
            }
            
            sleep(opts.interval);
        }
    } else {
        // 单次检查
        MTDMonitor::MTDStatus status = 
            MTDMonitor::check_partition_status(opts.device, opts.threshold);
        
        printf("MTD Partition Status:\n");
        printf("Device: %s\n", opts.device);
        printf("Total Size: %.2f MB\n", (float)status.total_size / (1024*1024));
        printf("Used Size: %.2f MB\n", (float)status.used_size / (1024*1024));
        printf("Available Size: %.2f MB\n", (float)status.available_size / (1024*1024));
        printf("Usage: %.1f%%\n", status.usage_percent);
        printf("Available: %.1f%%\n", status.available_percent);
        
        if (status.needs_format) {
            printf("Warning: Available space below threshold (%.1f%%)!\n", 
                   opts.threshold);
            
            if (opts.force_format) {
                printf("Formatting partition...\n");
                if (MTDMonitor::format_partition(opts.device)) {
                    printf("Format successful\n");
                } else {
                    printf("Format failed\n");
                }
            } else {
                char response;
                printf("Format partition? (y/n): ");
                scanf(" %c", &response);
                if (response == 'y' || response == 'Y') {
                    if (MTDMonitor::format_partition(opts.device)) {
                        printf("Format successful\n");
                    } else {
                        printf("Format failed\n");
                    }
                }
            }
        }
    }

    return 0;
}
