#pragma once

#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <sstream>
#include <thread>
#include <fcntl.h>
#include <functional>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <ctime>
#include <map>

class MWH_Frame {
private:

public:
    int InotifyMain(const char* dir_name, uint32_t mask) {
        // 这个是监听文件变化的函数 dir_name是路径 mask是监听类型
        int fd = inotify_init();
        if (fd < 0) {
            printf("Failed to initialize inotify.\n");
            return -1;
        }
 
       int wd = inotify_add_watch(fd, dir_name, mask);
        if (wd < 0) {
            printf("Failed to add watch for directory: %s \n", dir_name);
            close(fd);
            return -1;
        }

        const int buflen = sizeof(struct inotify_event) + NAME_MAX + 1;
        char buf[buflen];
        fd_set readfds;

        while (true) {
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);

            int iRet = select(fd + 1, &readfds, nullptr, nullptr, nullptr);
            if (iRet < 0) {
                break;
            }

            int len = read(fd, buf, buflen);
            if (len < 0) {
                printf ("Failed to read inotify events.\n");
                break;
            }

            const struct inotify_event* event = reinterpret_cast<const struct inotify_event*>(buf);
            if (event->mask & mask) {
                break; //文件发生变化时的处理逻辑
            }
        }

        inotify_rm_watch(fd, wd);
        close(fd);

        return 0;
    }
    void WriteFile(const std::string& filePath, const std::string& content) noexcept {
        // 这个是写入文件的 filePath路径 content参数
        int fd = open(filePath.c_str(), O_WRONLY | O_NONBLOCK);

        if (fd < 0) {
            chmod(filePath.c_str(), 0666);
            fd = open(filePath.c_str(), O_WRONLY | O_NONBLOCK); 
        }

        if (fd >= 0) {
            write(fd, content.data(), content.size());
            close(fd);
            chmod(filePath.c_str(), 0444);
        }
    }
    std::string exec(const std::string& command) {
        // 执行系统命令的popen管道
        char buffer[128];
        std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
        if (!pipe) {
            printf("open popen failed!\n");
            return "";
        }
        
        if (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
            result = buffer; 
        }

        return result;  
    }
    std::string getPids(const std::vector<std::string>& processNames) {
        DIR* dir = opendir("/proc");
        if (dir == nullptr) {
            printf("错误:无法打开/proc 目录!\n");
            return "";
        }

        std::ostringstream Pids;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_DIR && std::all_of(entry->d_name, entry->d_name + strlen(entry->d_name), ::isdigit)) {
                pid_t pid = static_cast<pid_t>(std::stoi(entry->d_name));
                std::string cmdlinePath = "/proc/" + std::string(entry->d_name) + "/cmdline";

                std::ifstream cmdlineFile(cmdlinePath);
                if (cmdlineFile) {
                    std::string cmdline;
                    std::getline(cmdlineFile, cmdline, '\0'); 
                    for (const auto& processName : processNames) {
                        if (cmdline.find(processName) != std::string::npos) {
                            Pids << pid << '\n';
                            break; 
                        }
                    }
                }
            }
        }
        closedir(dir);
        return Pids.str();
    }
    std::string ReadFile(const std::string& filePath) noexcept {
        int fd = open(filePath.c_str(), O_RDONLY);
        if (fd == -1) {
            return ""; 
        }

        struct stat sb;
        if (fstat(fd, &sb) == -1 || !S_ISREG(sb.st_mode)) {
            close(fd); 
            return ""; 
        }

        char* map = static_cast<char*>(mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
        if (map == MAP_FAILED) {
            close(fd); 
            return ""; 
        }
    
        std::string content(map, sb.st_size);
        munmap(map, sb.st_size);
        close(fd);

       return content;
    }
int main(){
    printf("Hello, World!\n");
    return 0;
}
};