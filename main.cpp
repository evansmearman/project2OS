#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits>
#include <fstream>
#include <pwd.h>
#include <grp.h>

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) tokens.push_back(tok);
    return tokens;
}
void cmd_help() {
    std::cout <<
        "myshell — built-in commands\n"
        "─────────────────────────────────────────\n"
        "  cd [dir]          Change working directory (default: HOME)\n"
        "  ls [dir]          List directory contents (names only)\n"
        "  dir [dir]         List directory with size and type\n"
        "  pwd               Print working directory\n"
        "  mkdir <dir>       Create directory\n"
        "  rmdir <dir>       Remove empty directory\n"
        "  rm <file>...      Remove file(s)\n"
        "  cp <src> <dest>   Copy file\n"
        "  mv <src> <dest>   Move / rename file\n"
        "  touch <file>...   Create empty file(s)\n"
        "  cat <file>...     Print file contents\n"
        "  grep <pat> <file> Search for pattern in file\n"
        "  wc <file>...      Count lines, words, bytes\n"
        "  echo [text]       Print text\n"
        "  environ           Print environment variables\n"
        "  chmod <mode> <f>  Change file permissions (octal)\n"
        "  chown <own> <f>   Change file owner (user[:group])\n"
        "  clr               Clear the screen\n"
        "  pause             Wait for Enter key\n"
        "  help              Show this help message\n"
        "  quit              Exit the shell\n"
        "─────────────────────────────────────────\n";
}
void cmd_cd(const std::vector<std::string>& args) {
    const char* target;
    if (args.size() < 2) {
        target = getenv("HOME");
        if (!target) { std::cerr << "cd: HOME not set\n"; return; }
    } else {
        target = args[1].c_str();
    }
    if (chdir(target) != 0) perror("cd");
}
void cmd_dir(const std::vector<std::string>& args) {
    const char* path = (args.size() > 1) ? args[1].c_str() : ".";
    DIR* dir = opendir(path);
    if (!dir) { perror("dir"); return; }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string fullPath = std::string(path) + "/" + name;
        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) { std::cout << name << "\n"; continue; }

        char typeChar = '-';
        if (S_ISDIR(st.st_mode))      typeChar = 'd';
        else if (S_ISLNK(st.st_mode)) typeChar = 'l';

        std::cout << typeChar << "  " << st.st_size << "\t" << name << "\n";
    }
    closedir(dir);
}
void cmd_mv(const std::vector<std::string>& args) {
    if (args.size() < 3) { std::cerr << "mv: missing file operand\n"; return; }

    std::string src  = args[1];
    std::string dest = args[2];

    struct stat destStat;
    if (stat(dest.c_str(), &destStat) == 0 && S_ISDIR(destStat.st_mode)) {
        size_t pos = src.find_last_of('/');
        std::string filename = (pos == std::string::npos) ? src : src.substr(pos + 1);
        dest += "/" + filename;
    }

    if (rename(src.c_str(), dest.c_str()) == 0) return;

    if (errno == EXDEV) {
        std::ifstream in(src, std::ios::binary);
        if (!in) { std::cerr << "mv: cannot open " << src << "\n"; return; }
        std::ofstream out(dest, std::ios::binary);
        if (!out) { std::cerr << "mv: cannot create " << dest << "\n"; return; }
        out << in.rdbuf();
        if (!out) { std::cerr << "mv: write error\n"; return; }
        if (unlink(src.c_str()) != 0) perror("mv: unlink");
    } else {
        perror("mv");
    }
}
void cmd_chown(const std::vector<std::string>& args) {
    if (args.size() < 3) { std::cerr << "chown: usage: chown <owner>[:<group>] <file>\n"; return; }

    std::string ownerArg = args[1];
    std::string filePath = args[2];

    std::string userName, groupName;
    size_t colonPos = ownerArg.find(':');
    if (colonPos != std::string::npos) {
        userName  = ownerArg.substr(0, colonPos);
        groupName = ownerArg.substr(colonPos + 1);
    } else {
        userName = ownerArg;
    }

    uid_t uid = (uid_t)-1;
    if (!userName.empty()) {
        struct passwd* pw = getpwnam(userName.c_str());
        if (!pw) {
            try { uid = (uid_t)std::stoul(userName); }
            catch (...) { std::cerr << "chown: invalid user: " << userName << "\n"; return; }
        } else {
            uid = pw->pw_uid;
        }
    }

    gid_t gid = (gid_t)-1;
    if (!groupName.empty()) {
        struct group* gr = getgrnam(groupName.c_str());
        if (!gr) {
            try { gid = (gid_t)std::stoul(groupName); }
            catch (...) { std::cerr << "chown: invalid group: " << groupName << "\n"; return; }
        } else {
            gid = gr->gr_gid;
        }
    }

    if (lchown(filePath.c_str(), uid, gid) != 0) perror("chown");
}
void cmd_pwd() {
    char buf[4096];
    if (getcwd(buf, sizeof(buf))) std::cout << buf << "\n";
    else std::cerr << "pwd: " << strerror(errno) << "\n";
}

void cmd_echo(const std::vector<std::string>& args) {
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) std::cout << " ";
        std::cout << args[i];
    }
    std::cout << "\n";
}

void cmd_pause() {
    std::cout << "Press Enter to continue...";
    std::cin.get();
}

void cmd_mkdir(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "mkdir: missing operand\n";
        return;
    }

    if (mkdir(args[1].c_str(), 0755) != 0) {
        perror("mkdir");
    }
}

void cmd_rmdir(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "rmdir: missing operand\n";
        return;
    }

    if (rmdir(args[1].c_str()) != 0) {
        perror("rmdir");
    }
}

void cmd_touch(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "touch: missing file operand\n";
        return;
    }

    for (size_t i = 1; i < args.size(); ++i) {
        std::ofstream file(args[i]);
        if (!file) {
            std::cerr << "touch: cannot create " << args[i] << "\n";
        }
    }
}

void cmd_rm(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "rm: missing operand\n";
        return;
    }

    for (size_t i = 1; i < args.size(); ++i) {
        if (unlink(args[i].c_str()) != 0) {
            perror(("rm: " + args[i]).c_str());
        }
    }
}

void cmd_ls(const std::vector<std::string>& args) {
    const char* path = (args.size() > 1) ? args[1].c_str() : ".";

    DIR* dir = opendir(path);
    if (!dir) {
        perror("ls");
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;

        if (name == "." || name == "..") continue;

        std::cout << name << "\n";
    }

    closedir(dir);
}

void cmd_cp(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "cp: missing file operand\n";
        return;
    }

    std::string srcPath = args[1];
    std::string destPath = args[2];

    std::ifstream src(srcPath, std::ios::binary);
    if (!src) {
        std::cerr << "cp: cannot open source file\n";
        return;
    }

    struct stat destStat;

    if (stat(destPath.c_str(), &destStat) == 0 && S_ISDIR(destStat.st_mode)) {
        size_t pos = srcPath.find_last_of('/');
        std::string filename = (pos == std::string::npos) ? srcPath : srcPath.substr(pos + 1);

        destPath += "/" + filename;
    }

    std::ofstream dest(destPath, std::ios::binary);
    if (!dest) {
        std::cerr << "cp: cannot open destination file\n";
        return;
    }

    dest << src.rdbuf();
}

void cmd_grep(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "grep: usage: grep <pattern> <file>\n";
        return;
    }

    std::string pattern = args[1];
    std::ifstream file(args[2]);

    if (!file) {
        std::cerr << "grep: cannot open file\n";
        return;
    }

    std::string line;
    int lineNumber = 1;

    while (std::getline(file, line)) {
        if (line.find(pattern) != std::string::npos) {
            std::cout << line << "\n";
        }
        lineNumber++;
    }
}

void cmd_cat(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "cat: missing file operand\n";
        return;
    }

    for (size_t i = 1; i < args.size(); ++i) {
        std::ifstream file(args[i]);

        if (!file) {
            std::cerr << "cat: cannot open " << args[i] << "\n";
            continue;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::cout << line << "\n";
        }
    }
}

void cmd_chmod(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "chmod: usage: chmod <mode> <file>\n";
        return;
    }

    std::string modeStr = args[1];
    std::string filePath = args[2];

    mode_t mode = 0;
    try {
        mode = std::stoul(modeStr, nullptr, 8);
    } catch (...) {
        std::cerr << "chmod: invalid mode\n";
        return;
    }

    if (chmod(filePath.c_str(), mode) != 0) {
        perror("chmod");
    }
}

void cmd_clr() {
    std::cout << "\033[H\033[2J\033[3J" << std::flush;
}
extern char** environ;

void cmd_environ() {
    for (char** env = environ; *env != nullptr; ++env)
        std::cout << *env << "\n";
}
void cmd_wc(const std::vector<std::string>& args) {
    if (args.size() < 2) { std::cerr << "wc: missing file operand\n"; return; }

    long totalLines = 0, totalWords = 0, totalBytes = 0;
    bool multiple = (args.size() > 2);

    for (size_t i = 1; i < args.size(); ++i) {
        std::ifstream file(args[i]);
        if (!file) { std::cerr << "wc: cannot open " << args[i] << "\n"; continue; }

        long lines = 0, words = 0, bytes = 0;
        std::string line;
        while (std::getline(file, line)) {
            ++lines;
            bytes += (long)line.size() + 1;
            std::istringstream ss(line);
            std::string word;
            while (ss >> word) ++words;
        }

        std::cout << lines << " " << words << " " << bytes << " " << args[i] << "\n";
        totalLines += lines; totalWords += words; totalBytes += bytes;
    }

    if (multiple)
        std::cout << totalLines << " " << totalWords << " " << totalBytes << " total\n";
}
int main() {
    std::string line;

    while (true) {
        std::cout << "myshell> " << std::flush;

        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        auto args = tokenize(line);
        if (args.empty()) continue;

        const std::string& cmd = args[0];

        if      (cmd == "quit")  break;
        else if (cmd == "pwd")   cmd_pwd();
        else if (cmd == "echo")  cmd_echo(args);
        else if (cmd == "pause") cmd_pause();
        else if (cmd == "mkdir") cmd_mkdir(args);
        else if (cmd == "rmdir") cmd_rmdir(args);
        else if (cmd == "touch") cmd_touch(args);
        else if (cmd == "rm")    cmd_rm(args);
        else if (cmd == "ls")    cmd_ls(args);
        else if (cmd == "cp")    cmd_cp(args);
        else if (cmd == "grep")  cmd_grep(args);
        else if (cmd == "cat")   cmd_cat(args);
        else if (cmd == "chmod") cmd_chmod(args);
        else if (cmd == "clr")   cmd_clr();
        else if (cmd == "cd")      cmd_cd(args);
        else if (cmd == "dir")     cmd_dir(args);
        else if (cmd == "environ") cmd_environ();
        else if (cmd == "help")    cmd_help();
        else if (cmd == "mv")      cmd_mv(args);
        else if (cmd == "wc")      cmd_wc(args);
        else if (cmd == "chown")   cmd_chown(args);
        else std::cerr << cmd << ": command not found\n";
    }

    return 0;
}
