#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <cerrno>
#include <unistd.h>

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) tokens.push_back(tok);
    return tokens;
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
        else std::cerr << cmd << ": command not found\n";
    }

    return 0;
}