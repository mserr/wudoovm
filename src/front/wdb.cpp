#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include "../version.h"
#include "../bytecode/maps.h"
#include "../support/string.h"
#include "../cpu/debugger.h"
#include "../program.h"
using namespace std;


const char* NOTE_LOADED_ASM = "note: seems like you have loaded an .asm file which cannot be run on CPU without prior compilation";
const char* RC_FILENAME = "/.wudoo.db.rc";


// MISC FLAGS
bool SHOW_HELP = false;
bool SHOW_VERSION = false;
bool VERBOSE = false;
bool DEBUG = false;
bool STEP_BY_STEP = false;


// WARNING FLAGS
bool WARNING_ALL = false;


// ERROR FLAGS
bool ERROR_ALL = false;


string join(const string& s, const vector<string>& parts) {
    ostringstream oss;
    unsigned limit = parts.size();
    for (unsigned i = 0; i < limit; ++i) {
        oss << parts[i];
        if (i < (limit-1)) {
            oss << s;
        }
    }
    return oss.str();
}

void debuggerMainLoop(CPU& cpu, deque<string> init) {
    vector<byte*> breakpoints_byte;
    vector<string> breakpoints_opcode;

    deque<string> command_feed = init;
    string line(""), partline(""), lastline("");
    string command("");
    vector<string> operands;
    vector<string> chunks;

    bool started = false;

    int ticks_left = 0;
    bool paused = false;

    while (true) {
        if (not command_feed.size()) {
            cout << ">>> ";
            getline(cin, line);
            line = str::lstrip(line);
        }

        if (line == string("{")) {
            while (true) {
                cout << "...  ";
                getline(cin, partline);
                if (partline == string("}")) {
                    break;
                }
                command_feed.push_back(partline);
            }
            partline = "";
        }

        if (command_feed.size()) {
            line = command_feed.front();
            command_feed.pop_front();
        }

        if (line == ".") {
            line = lastline;
        }
        if (line != "") {
            lastline = line;
        }

        command = str::chunk(line);
        operands = str::chunks(str::sub(line, command.size()));

        cout << "command:  `" << command << "`" << endl;
        cout << "operands: ";
        if (operands.size()) {
            cout << "`" << join("`, `", operands) << "`";
        }
        cout << endl;

        if (command == "") {
            // do nothing...
        } else if (command == "conf.set") {
            if (not operands.size()) {
                cout << "error: missing operands: <key> [value]" << endl;
                continue;
            }

            if (operands[0] == "cpu.stepping") {
                if (operands.size() == 1 or (operands.size() > 1 and operands[1] == "true")) {
                    cpu.stepping = true;
                } else if (operands.size() > 1 and operands[1] == "false") {
                    cpu.stepping = false;
                } else {
                    cout << "error: invalid operand, expected 'true' of 'false'" << endl;
                }
            } else if (operands[0] == "cpu.debug") {
                if (operands.size() == 1 or (operands.size() > 1 and operands[1] == "true")) {
                    cpu.debug = true;
                } else if (operands.size() > 1 and operands[1] == "false") {
                    cpu.debug = false;
                } else {
                    cout << "error: invalid operand, expected 'true' of 'false'" << endl;
                }
            }
        } else if (command == "conf.get") {
        } else if (command == "conf.load") {
            cout << "FIXME/TODO" << endl;
        } else if (command == "conf.load.default") {
            cout << "FIXME/TODO" << endl;
        } else if (command == "conf.dump") {
            cout << "FIXME/TODO" << endl;
        } else if (command == "breakpoint.set.at") {
            if (not operands.size()) {
                cout << "warn: requires integer operand(s)" << endl;
            }
            for (unsigned j = 0; j < operands.size(); ++j) {
                if (str::isnum(operands[j])) {
                    breakpoints_byte.push_back(cpu.bytecode+stoi(operands[j]));
                } else {
                    cout << "warn: invalid operand, expected integer: " << operands[j] << endl;
                }
            }
        } else if (command == "breakpoint.set.on") {
            for (unsigned j = 0; j < operands.size(); ++j) {
                breakpoints_opcode.push_back(operands[j]);
            }
        } else if (command == "cpu.prepare") {
            cpu.iframe();
            cpu.begin();
        } else if (command == "cpu.run") {
            if (started) {
                cout << "error: program has already been started, use `resume` command instead" << endl;
                continue;
            }
            started = true;
            ticks_left = -1;
        } else if (command == "cpu.resume") {
            if (not paused) {
                cout << "error: execution has not been paused, cannot resume" << endl;
                continue;
            }
            paused = false;
        } else if (command == "cpu.tick") {
            if (operands.size() > 1) {
                cout << "error: invalid operand size, expected 0 or 1 operand but got " << operands.size() << endl;
                continue;
            }
            if (operands.size() == 1 and not str::isnum(operands[0])) {
                cout << "error: invalid operand, expected integer" << endl;
                continue;
            }
            ticks_left = (operands.size() ? stoi(operands[0]) : 1);
        } else if (command == "quit") {
            break;
        } else {
            cout << "unknown command: `" << command << "`" << endl;
        }

        if (not started) { continue; }

        while (ticks_left == -1 or ((ticks_left--) > 0)) {
            cpu.tick();
            if (find(breakpoints_byte.begin(), breakpoints_byte.end(), cpu.instruction_pointer) != breakpoints_byte.end()) {
                cout << "info: execution halted by byte breakpoint: " << cpu.instruction_pointer << endl;
                paused = true;
            }
            if (find(breakpoints_opcode.begin(), breakpoints_opcode.end(), OP_NAMES.at(OPCODE(*cpu.instruction_pointer))) != breakpoints_opcode.end()) {
                cout << "info: execution halted by opcode breakpoint: " << OP_NAMES.at(OPCODE(*cpu.instruction_pointer)) << endl;
                paused = true;
            }
            if (paused) { break; }
        }
    }
}


int main(int argc, char* argv[]) {
    // setup command line arguments vector
    vector<string> args;
    string option;
    for (int i = 1; i < argc; ++i) {
        option = string(argv[i]);
        if (option == "--help") {
            SHOW_HELP = true;
            continue;
        } else if (option == "--version") {
            SHOW_VERSION = true;
            continue;
        } else if (option == "--verbose") {
            VERBOSE = true;
            continue;
        } else if (option == "--debug") {
            DEBUG = true;
            continue;
        } else if (option == "--Wall") {
            WARNING_ALL = true;
            continue;
        } else if (option == "--Eall") {
            ERROR_ALL = true;
            continue;
        } else if (option == "--step") {
            STEP_BY_STEP = true;
            continue;
        }
        args.push_back(argv[i]);
    }

    if (SHOW_HELP or SHOW_VERSION) {
        cout << "wudoo VM virtual machine, version " << VERSION << endl;
        if (SHOW_HELP) {
            cout << "    --analyze          - to display information about loaded bytecode but not run it" << endl;
            cout << "    --debug <infile>   - to run a program in debug mode (shows debug output)" << endl;
            cout << "    --help             - to display this message" << endl;
            cout << "    --step <infile>    - to run a program in stepping mode (pauses after each instruction, implies debug mode for CPU)" << endl;
        }
        return 0;
    }

    if (args.size() == 0) {
        cout << "fatal: no input file" << endl;
        return 1;
    }

    string filename = "";
    filename = args[0];

    if (!filename.size()) {
        cout << "fatal: no file to run" << endl;
        return 1;
    }

    cout << "message: running \"" << filename << "\"" << endl;

    ifstream in(filename, ios::in | ios::binary);

    if (!in) {
        cout << "fatal: file could not be opened: " << filename << endl;
        return 1;
    }

    uint16_t function_ids_section_size = 0;
    char buffer[16];
    in.read(buffer, sizeof(uint16_t));
    function_ids_section_size = *((uint16_t*)buffer);
    if (VERBOSE or DEBUG) { cout << "message: function mapping section: " << function_ids_section_size << " bytes" << endl; }

    /*  The code below extracts function id-to-address mapping.
     */
    map<string, uint16_t> function_address_mapping;
    char *buffer_function_ids = new char[function_ids_section_size];
    in.read(buffer_function_ids, function_ids_section_size);
    char *function_ids_map = buffer_function_ids;

    int i = 0;
    string fn_name;
    uint16_t fn_address;
    while (i < function_ids_section_size) {
        fn_name = string(function_ids_map);
        i += fn_name.size() + 1;  // one for null character
        fn_address = *((uint16_t*)(buffer_function_ids+i));
        i += sizeof(uint16_t);
        function_ids_map = buffer_function_ids+i;
        function_address_mapping[fn_name] = fn_address;

        cout << "debug: function id-to-address mapping: " << fn_name << " @ byte " << fn_address << endl;
    }
    delete[] buffer_function_ids;


    uint16_t bytes;

    in.read(buffer, 16);
    if (!in) {
        cout << "fatal: an error occued during bytecode loading: cannot read size" << endl;
        if (str::endswith(filename, ".asm")) { cout << NOTE_LOADED_ASM << endl; }
        return 1;
    } else {
        bytes = *((uint16_t*)buffer);
    }
    cout << "message: bytecode size: " << bytes << " bytes" << endl;

    uint16_t starting_instruction = function_address_mapping["__entry"];
    cout << "message: first executable instruction at byte " << starting_instruction << endl;

    byte* bytecode = new byte[bytes];
    in.read((char*)bytecode, bytes);

    if (!in) {
        cout << "fatal: an error occued during bytecode loading: cannot read instructions" << endl;
        if (str::endswith(filename, ".asm")) { cout << NOTE_LOADED_ASM << endl; }
        return 1;
    }
    in.close();

    int ret_code = 0;
    string return_exception = "", return_message = "";
    // run the bytecode
    CPU cpu;
    cpu.debug = true;
    cpu.stepping = true;
    for (auto p : function_address_mapping) { cpu.mapfunction(p.first, p.second); }

    vector<string> cmdline_args;
    for (int i = 1; i < argc; ++i) {
        cmdline_args.push_back(argv[i]);
    }

    cpu.commandline_arguments = cmdline_args;
    cpu.load(bytecode).bytes(bytes).eoffset(starting_instruction);

    string homedir(getenv("HOME"));
    ifstream local_rc_file(homedir + RC_FILENAME);
    ifstream global_rc_file("/etc/wudoovm/dbrc");

    deque<string> init_commands;
    string line;
    if (global_rc_file) {
        while (getline(global_rc_file, line)) { init_commands.push_back(line); }
    }
    if (local_rc_file) {
        while (getline(local_rc_file, line)) { init_commands.push_back(line); }
    }

    debuggerMainLoop(cpu, init_commands);

    return ret_code;
}
