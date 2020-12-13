#include <unistd.h>
#include <sys/wait.h>
#include <cerrno>
#include <cstdio>

#include <string>
#include <sstream>
#include <iostream>
#include <array>
#include <vector>
#include <utility>

#include "json.hpp"

using json = nlohmann::json;

constexpr std::size_t buf_size = 1024;

void command(std::vector<char*>& args) {
    char **cmd = args.data();
    switch(fork()) {
        case -1:
            std::perror("command");
            exit(1);
        case 0:                 // child
            if(execvp(cmd[0], &cmd[0])==-1) {
                std::perror("command");
            }
            exit(0);
        default:                // parent
            wait(nullptr);
    }
}

std::string command_output(std::vector<char*>& args) {
    std::array<int,2> handles;
    std::array<char,buf_size> buffer;
    std::size_t num_read;
    char **cmd = args.data();
    std::string ret;
    ret.reserve(1024);

    if(pipe(handles.data())==-1) {
        std::cerr << "Can't create pipes.\n";
        exit(1);
    }

    switch(fork()) {
        case -1:
            std::perror("command_output");
            exit(1);
        case 0:                 // child
            dup2(handles.at(1), STDOUT_FILENO);
            if(close(handles.at(0))==-1 || close(handles.at(1))==-1) {
                std::perror("command_output");
                exit(1);
            }
            if(execvp(cmd[0], &cmd[0])==-1) {
                std::perror("command_output");
            }
            exit(0);
        default:                // parent
            if(close(handles.at(1))==-1) {
                std::perror("command_output");
                exit(1);
            }
            for (;;) {
                num_read = read(handles.at(0), buffer.data(), buf_size);
                if(num_read == -1) {
                    std::perror("command_output");
                    exit(1);
                }
                if(num_read == 0)
                    break;
                ret.append(buffer.data(), num_read);
            }
            if(close(handles.at(0))==-1) {
                std::perror("command_output");
                exit(1);
            }
            wait(nullptr);
    }
    return ret;
}

void command_input(std::vector<char*>& args, const std::string& in) {
    std::array<int,2> handles;
    std::array<char,buf_size> buffer;
    char **cmd = args.data();

    if(pipe(handles.data())==-1) {
        std::cerr << "Can't create pipes.\n";
        exit(1);
    }

    switch(fork()) {
        case -1:
            std::perror("command_input");
            exit(1);
        case 0:                 // child
            dup2(handles.at(0), STDIN_FILENO);
            if(close(handles.at(0))==-1 || close(handles.at(1))==-1) {
                std::perror("command_input");
                exit(1);
            }
            if(execvp(cmd[0], &cmd[0])==-1) {
                std::perror("command_input");
            }
            exit(0);
        default:                // parent
            if(close(handles.at(0))==-1) {
                std::perror("command_input");
                exit(1);
            }
            if(write(handles.at(1), in.c_str(), in.size())!=in.size()) {
                std::cerr << "command_input: failed/partial write\n";
                exit(1);
            }
            if(close(handles.at(1))==-1) {
                std::perror("command_input");
                exit(1);
            }
            wait(nullptr);
    }
}

std::string command_input_output(std::vector<char*>& args, const std::string& in) {
    std::array<int,2> in_handles, out_handles;
    std::array<char,buf_size> buffer;
    std::size_t num_read;
    char **cmd = args.data();
    std::string ret;
    ret.reserve(1024);

    if(pipe(in_handles.data())==-1) {
        std::cerr << "Can't create pipes.\n";
        exit(1);
    }
    if(pipe(out_handles.data())==-1) {
        std::cerr << "Can't create pipes.\n";
        exit(1);
    }

    switch(fork()) {
        case -1:
            std::perror("command_input_output");
            exit(1);
        case 0:                 // child
            dup2(in_handles.at(0), STDIN_FILENO);
            dup2(out_handles.at(1), STDOUT_FILENO);
            if(close(in_handles.at(0))==-1 || close(in_handles.at(1))==-1
               || close(out_handles.at(0))==-1 || close(out_handles.at(1))==-1) {
                std::perror("command_input_output");
                exit(1);
            }

            if(execvp(cmd[0], &cmd[0])==-1) {
                std::perror("command_input_output");
            }
            exit(0);
        default:                // parent
            if(close(in_handles.at(0))==-1 || close(out_handles.at(1))==-1) {
                std::perror("command_input_output");
                exit(1);
            }
            if(write(in_handles.at(1), in.c_str(), in.size())!=in.size()) {
                std::cerr << "command_input_output: failed/partial write\n";
                exit(1);
            }

            if(close(in_handles.at(1))==-1) {
                std::perror("command_input_output");
                exit(1);
            }
            for (;;) {
                num_read = read(out_handles.at(0), buffer.data(), buf_size);
                if(num_read == -1) {
                    std::perror("command_input_output");
                    exit(1);
                }
                if(num_read == 0)
                    break;
                ret.append(buffer.data(), num_read);
            }
            if(close(out_handles.at(0))==-1) {
                std::perror("command_input_output");
                exit(1);
            }

            wait(nullptr);
    }

    return ret;
}

std::vector<json> extract_leaf_nodes(json &w) {
    std::vector<json> leaves;
    for(auto &n : w["nodes"]) {
        if(n["nodes"].empty())
            leaves.push_back(n);
        else {
            std::vector<json> new_leaves = extract_leaf_nodes(n);
            leaves.insert(leaves.end(), new_leaves.begin(), new_leaves.end());
        }
    }

    return leaves;
}

std::vector<std::pair<int,json> > get_ws_windows() {
    std::vector<char*> sway_cmd;
    sway_cmd.push_back(const_cast<char *>("swaymsg"));
    sway_cmd.push_back(const_cast<char *>("-t"));
    sway_cmd.push_back(const_cast<char *>("get_tree"));
    sway_cmd.push_back(nullptr);

    std::string output = command_output(sway_cmd);
    std::vector<std::pair<int,json> > ws_windows;
    json sway_json = json::parse(output);
    int w_num = 0;
    for(auto &o : sway_json["nodes"]) {
        if(o["name"].get<std::string>() != "__i3" && o["type"].get<std::string>() == "output") {
            for(auto &w : o["nodes"]) {
                if(w["type"].get<std::string>() == "workspace") {
                    w_num++;
                    for(auto & wf : w["floating_nodes"])
                        ws_windows.emplace_back(w_num, wf);
                    std::vector<json> new_windows = extract_leaf_nodes(w);
                    for(auto & win : new_windows)
                        ws_windows.emplace_back(w_num, win);
                }
            }
        }
    }

    return ws_windows;
}

std::string show_wofi(const std::string& in) {
    std::vector<char*> sway_cmd;
    sway_cmd.push_back(const_cast<char *>("wofi"));
    sway_cmd.push_back(const_cast<char *>("-p"));
    sway_cmd.push_back(const_cast<char *>("Windows: "));
    sway_cmd.push_back(const_cast<char *>("-d"));
    sway_cmd.push_back(const_cast<char *>("-i"));
    sway_cmd.push_back(const_cast<char *>("-m"));
    sway_cmd.push_back(const_cast<char *>("-k"));
    sway_cmd.push_back(const_cast<char *>("/dev/null"));
    sway_cmd.push_back(const_cast<char *>("--hide-scroll"));
    sway_cmd.push_back(nullptr);

    return command_input_output(sway_cmd, in);
}

std::string extract_id(const std::string& title) {
    auto idx = title.rfind('[');
    return title.substr(idx+1, title.size()-2-idx-1);
}

void switch_window(const std::string & con_id) {
    std::vector<char*> sway_cmd;
    std::string sarg = "[con_id=";
    sarg.append(con_id);
    sarg.append("]");
    sway_cmd.push_back(const_cast<char *>("swaymsg"));
    sway_cmd.push_back(const_cast<char *>(sarg.c_str()));
    sway_cmd.push_back(const_cast<char *>("focus"));
    sway_cmd.push_back(nullptr);

    command(sway_cmd);
}

int main(int argc, char *argv[]) {
    std::vector<std::pair<int,json> > ws_windows = get_ws_windows();
    std::stringstream wofi_input;

    for(auto &w : ws_windows) {
        wofi_input << "<b>" << w.first << ": " << "</b>" << w.second["name"].get<std::string>() << " [" << w.second["id"].get<int>() << "]" << std::endl;
    }

    std::string output = show_wofi(wofi_input.str());
    std::string con_id = extract_id(output);
    switch_window(con_id);

    return 0;
}
