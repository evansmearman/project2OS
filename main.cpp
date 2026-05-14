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
#include <utime.h>

extern char** environ;

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) tokens.push_back(tok);
    return tokens;
}

void cmd_help() {
    std::vector<std::string> lines = {
        "  cd [dir]          Change working directory (default: prints current directory)",
        "  ls [dir]          List directory contents (names only)",
        "  dir [dir]         List directory with size and type",
        "  pwd               Print working directory",
        "  mkdir <dir>       Create directory",
        "  rmdir <dir>       Remove empty directory",
        "  rm <file>...      Remove file(s)",
        "  cp <src> <dest>   Copy file",
        "  mv <src> <dest>   Move / rename file",
        "  touch <file>...   Create empty file(s)",
        "  cat <file>...     Print file contents",
        "  grep <pat> <file> Search for pattern in file",
        "  wc <file>...      Count lines, words, bytes",
        "  echo [text]       Print text",
        "  environ           Print environment variables",
        "  chmod <mode> <f>  Change file permissions (octal)",
        "  chown <own> <f>   Change file owner (user[:group])",
        "  clr               Clear the screen",
        "  pause             Wait for Enter key",
        "  help              Show this help message",
        "  quit              Exit the shell"
    };

    for (size_t i = 0; i < lines.size(); i++) {
        std::cout << lines[i] << "\n";

        if ((i + 1) % 5 == 0) {
            std::cout << "--More--";
            std::cin.get();
        }
    }
}

void cmd_cd(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        char buf[4096];
        if (getcwd(buf, sizeof(buf)))
            std::cout << buf << "\n";
        return;
    }

    if (chdir(args[1].c_str()) != 0) {
        perror("cd");
        return;
    }

    char buf[4096];
    if (getcwd(buf, sizeof(buf)))
        setenv("PWD", buf, 1);
}

void cmd_mkdir(const std::vector<std::string>& args) {
    for (size_t i = 1; i < args.size(); i++) {
        if (mkdir(args[i].c_str(), 0755) != 0)
            perror(("mkdir: " + args[i]).c_str());
    }
}

void cmd_rmdir(const std::vector<std::string>& args) {
    for (size_t i = 1; i < args.size(); i++) {
        if (rmdir(args[i].c_str()) != 0)
            perror(("rmdir: " + args[i]).c_str());
    }
}

void remove_recursive(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return;

    if (S_ISDIR(st.st_mode)) {
        DIR* dir = opendir(path.c_str());
        struct dirent* entry;

        while ((entry = readdir(dir))) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            remove_recursive(path + "/" + name);
        }
        closedir(dir);
        rmdir(path.c_str());
    } else {
        unlink(path.c_str());
    }
}

void cmd_rm(const std::vector<std::string>& args) {
    for (size_t i = 1; i < args.size(); i++)
        remove_recursive(args[i]);
}

void copy_file(const std::string& src, const std::string& dest) {
    std::ifstream in(src, std::ios::binary);

    if (!in) {
        std::cerr << "cp: cannot open source file\n";
        return;
    }

    std::ofstream out(dest, std::ios::binary);

    if (!out) {
        std::cerr << "cp: cannot open destination file\n";
        return;
    }

    out << in.rdbuf();

    in.close();
    out.close();
}

void copy_recursive(const std::string& src, const std::string& dest) {
    struct stat st;
    stat(src.c_str(), &st);

    if (S_ISDIR(st.st_mode)) {
        mkdir(dest.c_str(), 0755);

        DIR* dir = opendir(src.c_str());
        struct dirent* entry;

        while ((entry = readdir(dir))) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;

            copy_recursive(src + "/" + name, dest + "/" + name);
        }
        closedir(dir);
    } else {
        copy_file(src, dest);
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

    std::string src = args[1];
    std::string dest = args[2];

    struct stat st;

    if (stat(dest.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        size_t pos = src.find_last_of('/');

        std::string filename;

        if (pos == std::string::npos)
            filename = src;
        else
            filename = src.substr(pos + 1);

        if (dest.back() != '/')
            dest += "/";

        dest += filename;
    }

    copy_recursive(src, dest);
}

void cmd_mv(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "mv: missing file operand\n";
        return;
    }

    std::string src = args[1];
    std::string dest = args[2];

    struct stat st;

    if (stat(dest.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        size_t pos = src.find_last_of('/');

        std::string filename;

        if (pos == std::string::npos)
            filename = src;
        else
            filename = src.substr(pos + 1);

        if (dest.back() != '/')
            dest += "/";

        dest += filename;
    }

    if (rename(src.c_str(), dest.c_str()) == 0)
        return;

    copy_recursive(src, dest);

    if (stat(dest.c_str(), &st) == 0)
        remove_recursive(src);
    else
        std::cerr << "mv: failed to move file\n";
}

void cmd_touch(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "touch: missing file operand\n";
        return;
    }

    for (size_t i = 1; i < args.size(); i++) {
        struct stat st;

        if (stat(args[i].c_str(), &st) == 0) {
            if (utime(args[i].c_str(), NULL) != 0)
                perror("touch");
        } else {
            std::ofstream file(args[i]);

            if (!file)
                std::cerr << "touch: cannot create " << args[i] << "\n";

            file.close();
        }
    }
}

void cmd_grep(const std::vector<std::string>& args) {
    if (args.size() < 2) return;

    std::string pattern = args[1];
    std::istream* input = &std::cin;
    std::ifstream file;

    if (args.size() >= 3) {
        file.open(args[2]);
        input = &file;
    }

    std::string line;
    while (std::getline(*input, line)) {
        if (line.find(pattern) != std::string::npos)
            std::cout << line << "\n";
    }
}

void cmd_wc(const std::vector<std::string>& args) {
    std::istream* input = &std::cin;
    std::ifstream file;

    if (args.size() >= 2) {
        file.open(args[1]);
        input = &file;
    }

    long lines = 0, words = 0, chars = 0;
    std::string line;

    while (std::getline(*input, line)) {
        lines++;
        chars += line.size() + 1;

        std::istringstream ss(line);
        std::string word;
        while (ss >> word) words++;
    }

    std::cout << lines << " " << words << " " << chars << "\n";
}

void cmd_pwd() {
    char buf[4096];
    getcwd(buf, sizeof(buf));
    std::cout << buf << "\n";
}

void cmd_echo(const std::vector<std::string>& args) {
    for (size_t i = 1; i < args.size(); i++) {
        if (i > 1) std::cout << " ";
        std::cout << args[i];
    }
    std::cout << "\n";
}

void cmd_pause() {
    std::cout << "Press Enter to continue...";
    std::cin.get();
}

void cmd_clr() {
    std::cout << "\033[H\033[2J\033[3J" << std::flush;
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

void cmd_cat(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "cat: missing file operand\n";
        return;
    }

    for (size_t i = 1; i < args.size(); i++) {
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

    mode_t mode = 0;

    try {
        mode = std::stoul(args[1], nullptr, 8);
    } catch (...) {
        std::cerr << "chmod: invalid mode\n";
        return;
    }

    if (chmod(args[2].c_str(), mode) != 0)
        perror("chmod");
}
void cmd_up(const std::vector<std::string>& args) {
    int levels = 1;
    if (args.size() > 1) {
        try { levels = std::stoi(args[1]); }
        catch (...) { std::cerr << "up: invalid number\n"; return; }
    }
    for (int i = 0; i < levels; i++) {
        if (chdir("..") != 0) {
            perror("up");
            return;
        }
    }
    char buf[4096];
    if (getcwd(buf, sizeof(buf))) {
        setenv("PWD", buf, 1);
        std::cout << buf << "\n";
    }
}
void cmd_chown(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "chown: usage: chown <owner>[:group] <file>\n";
        return;
    }

    std::string ownerArg = args[1];
    std::string filePath = args[2];

    std::string userName, groupName;

    size_t colonPos = ownerArg.find(':');

    if (colonPos != std::string::npos) {
        userName = ownerArg.substr(0, colonPos);
        groupName = ownerArg.substr(colonPos + 1);
    } else {
        userName = ownerArg;
    }

    uid_t uid = (uid_t)-1;
    gid_t gid = (gid_t)-1;

    if (!userName.empty()) {
        struct passwd* pw = getpwnam(userName.c_str());

        if (pw)
            uid = pw->pw_uid;
    }

    if (!groupName.empty()) {
        struct group* gr = getgrnam(groupName.c_str());

        if (gr)
            gid = gr->gr_gid;
    }

    if (chown(filePath.c_str(), uid, gid) != 0)
        perror("chown");
}

void cmd_environ() {
    for (char** env = environ; *env; env++)
        std::cout << *env << "\n";
}

int main() {
    std::string line;

    while (true) {
        std::cout << "myshell> ";

        if (!std::getline(std::cin, line)) break;
        auto args = tokenize(line);
        if (args.empty()) continue;

        std::string cmd = args[0];

        if      (cmd == "quit")    break;
        else if (cmd == "cd")      cmd_cd(args);
        else if (cmd == "pwd")     cmd_pwd();
        else if (cmd == "mkdir")   cmd_mkdir(args);
        else if (cmd == "rmdir")   cmd_rmdir(args);
        else if (cmd == "rm")      cmd_rm(args);
        else if (cmd == "ls")      cmd_ls(args);
        else if (cmd == "cp")      cmd_cp(args);
        else if (cmd == "mv")      cmd_mv(args);
        else if (cmd == "touch")   cmd_touch(args);
        else if (cmd == "cat")     cmd_cat(args);
        else if (cmd == "grep")    cmd_grep(args);
        else if (cmd == "wc")      cmd_wc(args);
        else if (cmd == "echo")    cmd_echo(args);
        else if (cmd == "pause")   cmd_pause();
        else if (cmd == "clr")     cmd_clr();
        else if (cmd == "dir")     cmd_dir(args);
        else if (cmd == "chmod")   cmd_chmod(args);
        else if (cmd == "chown")   cmd_chown(args);
        else if (cmd == "environ") cmd_environ();
        else if (cmd == "help")    cmd_help();
        else if (cmd == "up")      cmd_up(args);
        else std::cerr << cmd << ": command not found\n";
    }
}
