#include <iostream>
#include <vector>
#include "../bytecode/bytetypedef.h"
#include "../bytecode/opcodes.h"
#include "../bytecode/maps.h"
#include "../types/object.h"
#include "../types/integer.h"
#include "../types/byte.h"
#include "../types/string.h"
#include "../types/vector.h"
#include "../support/pointer.h"
#include "cpu.h"
using namespace std;


CPU& CPU::load(byte* bc) {
    /*  Load bytecode into the CPU.
     *  CPU becomes owner of loaded bytecode - meaning it will consider itself responsible for proper
     *  destruction of it, so make sure you have a copy of the bytecode.
     *
     *  Any previously loaded bytecode is freed.
     *  To free bytecode without loading anything new it is possible to call .load(0).
     *
     *  :params:
     *
     *  bc:char*    - pointer to byte array containing bytecode with a program to run
     */
    if (bytecode) { delete[] bytecode; }
    bytecode = bc;
    return (*this);
}

CPU& CPU::bytes(uint16_t sz) {
    /*  Set bytecode size, so the CPU can stop execution even if it doesn't reach HALT instruction but reaches
     *  bytecode address out of bounds.
     */
    bytecode_size = sz;
    return (*this);
}

CPU& CPU::eoffset(uint16_t o) {
    /*  Set offset of first executable instruction.
     */
    executable_offset = o;
    return (*this);
}

CPU& CPU::mapfunction(const string& name, unsigned address) {
    /** Maps function name to bytecode address.
     */
    function_addresses[name] = address;
    return (*this);
}


Object* CPU::fetch(int index) {
    /*  Return pointer to object at given register.
     *  This method safeguards against reaching for out-of-bounds registers and
     *  reading from an empty register.
     *
     *  :params:
     *
     *  index:int   - index of a register to fetch
     */
    if (index >= uregisters_size) { throw "register access out of bounds: read"; }
    Object* optr = uregisters[index];
    if (optr == 0) {
        ostringstream oss;
        oss << "read from null register: " << index;
        throw oss.str().c_str();
    }
    return optr;
}


template<class T> inline void copyvalue(Object* a, Object* b) {
    /** This is a short inline, template function to copy value between two `Object` pointers of the same polymorphic type.
     *  It is used internally by CPU.
     */
    static_cast<T>(a)->value() = static_cast<T>(b)->value();
}

void CPU::updaterefs(Object* before, Object* now) {
    /** This method updates references to a given address present in registers.
     *  It swaps old address for the new one in every register that points to the old address and
     *  is marked as a reference.
     */
    for (int i = 0; i < uregisters_size; ++i) {
        if (uregisters[i] == before and ureferences[i]) {
            if (debug) {
                cout << "\nCPU: updating reference address in register " << i << hex << ": 0x" << (unsigned long)before << " -> 0x" << (unsigned long)now << dec << endl;
            }
            uregisters[i] = now;
        }
    }
}

bool CPU::hasrefs(int index) {
    /** This method checks if object at a given address exists as a reference in another register.
     */
    bool has = false;
    for (int i = 0; i < uregisters_size; ++i) {
        if (i == index) continue;
        if (uregisters[i] == uregisters[index]) {
            has = true;
            break;
        }
    }
    return has;
}

void CPU::place(int index, Object* obj) {
    /** Place an object in register with given index.
     *
     *  Before placing an object in register, a check is preformed if the register is empty.
     *  If not - the `Object` previously stored in it is destroyed.
     *
     */
    if (index >= uregisters_size) { throw "register access out of bounds: write"; }
    if (uregisters[index] != 0 and !ureferences[index]) {
        // register is not empty and is not a reference - the object in it must be destroyed to avoid memory leaks
        delete uregisters[index];
    }
    if (ureferences[index]) {
        Object* referenced = fetch(index);

        // it is a reference, copy value of the object
        if (referenced->type() == "Integer") { copyvalue<Integer*>(referenced, obj); }
        else if (referenced->type() == "Byte") { copyvalue<Byte*>(referenced, obj); }

        // and delete the newly created object to avoid leaks
        delete obj;
    } else {
        Object* old_ref_ptr = (hasrefs(index) ? uregisters[index] : 0);
        uregisters[index] = obj;
        if (old_ref_ptr) { updaterefs(old_ref_ptr, obj); }
    }
}

void CPU::ensureStaticRegisters(string function_name) {
    /** Makes sure that static register set for requested function is initialized.
     */
    try {
        static_registers.at(function_name);
    } catch (const std::out_of_range& e) {
        // 1) initialise all register set variables
        int s_registers_size = 16;  // FIXME: size of static register should be customisable
        Object** s_registers = new Object*[s_registers_size];
        bool* s_references = new bool[s_registers_size];

        // 2) zero registers and refrences
        for (int i = 0; i < s_registers_size; ++i) {
            s_registers[i] = 0;
            s_references[i] = false;
        }

        // 3) assign initialised register set to a function name
        static_registers[function_name] = tuple<Object**, bool*, int>(s_registers, s_references, s_registers_size);
    }
}

byte* CPU::begin() {
    /** Set instruction pointer to the execution beginning position.
     */
    return (instruction_pointer = bytecode+executable_offset);
}

CPU& CPU::iframe(Frame* frm) {
    /** Set initial frame.
     */
    Frame *initial_frame;
    if (frm == 0) {
        initial_frame = new Frame(0, 0, 0);
        Vector* cmdline = new Vector();
        for (unsigned i = 0; i < commandline_arguments.size(); ++i) {
            cmdline->push(new String(commandline_arguments[i]));
        }
        registers[1] = cmdline;
        initial_frame->registers = registers;
        initial_frame->references = references;
        initial_frame->registers_size = reg_count;
        initial_frame->function_name = "__entry";
    } else {
        initial_frame = frm;
    }
    uregisters = initial_frame->registers;
    ureferences = initial_frame->references;
    uregisters_size = initial_frame->registers_size;
    frames.push_back(initial_frame);

    return (*this);
}


byte* CPU::dispatch(byte* addr) {
    /** Dispatches instruction at a pointer to its handler.
     */
    switch (*addr) {
        case IZERO:
            addr = izero(addr+1);
            break;
        case ISTORE:
            addr = istore(addr+1);
            break;
        case IADD:
            addr = iadd(addr+1);
            break;
        case ISUB:
            addr = isub(addr+1);
            break;
        case IMUL:
            addr = imul(addr+1);
            break;
        case IDIV:
            addr = idiv(addr+1);
            break;
        case IINC:
            addr = iinc(addr+1);
            break;
        case IDEC:
            addr = idec(addr+1);
            break;
        case ILT:
            addr = ilt(addr+1);
            break;
        case ILTE:
            addr = ilte(addr+1);
            break;
        case IGT:
            addr = igt(addr+1);
            break;
        case IGTE:
            addr = igte(addr+1);
            break;
        case IEQ:
            addr = ieq(addr+1);
            break;
        case FSTORE:
            addr = fstore(addr+1);
            break;
        case FADD:
            addr = fadd(addr+1);
            break;
        case FSUB:
            addr = fsub(addr+1);
            break;
        case FMUL:
            addr = fmul(addr+1);
            break;
        case FDIV:
            addr = fdiv(addr+1);
            break;
        case FLT:
            addr = flt(addr+1);
            break;
        case FLTE:
            addr = flte(addr+1);
            break;
        case FGT:
            addr = fgt(addr+1);
            break;
        case FGTE:
            addr = fgte(addr+1);
            break;
        case FEQ:
            addr = feq(addr+1);
            break;
        case BSTORE:
            addr = bstore(addr+1);
            break;
        case ITOF:
            addr = itof(addr+1);
            break;
        case FTOI:
            addr = ftoi(addr+1);
            break;
        case STRSTORE:
            addr = strstore(addr+1);
            break;
        case VEC:
            addr = vec(addr+1);
            break;
        case VINSERT:
            addr = vinsert(addr+1);
            break;
        case VPUSH:
            addr = vpush(addr+1);
            break;
        case VPOP:
            addr = vpop(addr+1);
            break;
        case VAT:
            addr = vat(addr+1);
            break;
        case VLEN:
            addr = vlen(addr+1);
            break;
        case NOT:
            addr = lognot(addr+1);
            break;
        case AND:
            addr = logand(addr+1);
            break;
        case OR:
            addr = logor(addr+1);
            break;
        case MOVE:
            addr = move(addr+1);
            break;
        case COPY:
            addr = copy(addr+1);
            break;
        case REF:
            addr = ref(addr+1);
            break;
        case SWAP:
            addr = swap(addr+1);
            break;
        case FREE:
            addr = free(addr+1);
            break;
        case EMPTY:
            addr = empty(addr+1);
            break;
        case ISNULL:
            addr = isnull(addr+1);
            break;
        case RESS:
            addr = ress(addr+1);
            break;
        case TMPRI:
            addr = tmpri(addr+1);
            break;
        case TMPRO:
            addr = tmpro(addr+1);
            break;
        case PRINT:
            addr = print(addr+1);
            break;
        case ECHO:
            addr = echo(addr+1);
            break;
        case FRAME:
            addr = frame(addr+1);
            break;
        case PARAM:
            addr = param(addr+1);
            break;
        case PAREF:
            addr = paref(addr+1);
            break;
        case ARG:
            addr = arg(addr+1);
            break;
        case CALL:
            addr = call(addr+1);
            break;
        case END:
            addr = end(addr);
            break;
        case JUMP:
            addr = jump(addr+1);
            break;
        case BRANCH:
            addr = branch(addr+1);
            break;
        case HALT:
            throw HaltException();
            break;
        case PASS:
            ++addr;
            break;
        case NOP:
            ++addr;
            break;
        default:
            ostringstream error;
            error << "unrecognised instruction (bytecode value: " << *((int*)bytecode) << ")";
            throw error.str().c_str();
    }
    return addr;
}


byte* CPU::tick() {
    /** Perform a *tick*, i.e. run a single CPU instruction.
     */
    bool halt = false;
    byte* previous_instruction_pointer = instruction_pointer;
    ++instruction_counter;

    if (debug) {
        cout << "CPU: bytecode ";
        cout << dec << ((long)instruction_pointer - (long)bytecode);
        cout << " at 0x" << hex << (long)instruction_pointer;
        cout << dec << ": ";
    }

    try {
        if (debug) { cout << OP_NAMES.at(OPCODE(*instruction_pointer)); }
        instruction_pointer = dispatch(instruction_pointer);
        if (debug and not stepping) { cout << endl; }
    } catch (const char*& e) {
        return_code = 1;
        return_message = string(e);
        return_exception = "RuntimeException";
        return 0;
    } catch (const string& e) {
        return_code = 1;
        return_message = string(e);
        return_exception =  "RuntimeException";
        return 0;
    } catch (const HaltException& e) {
        halt = true;
    }

    if (halt or frames.size() == 0) { return 0; }

    if (instruction_pointer >= (bytecode+bytecode_size)) {
        return_code = 1;
        return_exception = "InvalidBytecodeAddress";
        return_message = string("instruction address out of bounds");
        return 0;
    }

    if (instruction_pointer == previous_instruction_pointer and OPCODE(*instruction_pointer) != END) {
        return_code = 2;
        ostringstream oss;
        return_exception = "InstructionUnchanged";
        oss << "instruction pointer did not change, possibly endless loop\n";
        oss << "note: instruction index was " << (long)(instruction_pointer-bytecode) << " and the opcode was '" << OP_NAMES.at(OPCODE(*instruction_pointer)) << "'";
        if (OPCODE(*instruction_pointer) == CALL) {
            oss << '\n';
            oss << "note: this was caused by 'call' opcode immediately calling itself\n"
                << "      such situation may have several sources, e.g. empty function definition or\n"
                << "      a function which calls itself in its first instruction";
        }
        return_message = oss.str();
        return 0;
    }

    if (stepping) {
        string ins;
        getline(cin, ins);
    }

    return instruction_pointer;
}

int CPU::run() {
    /*  VM CPU implementation.
     */
    if (!bytecode) {
        throw "null bytecode (maybe not loaded?)";
    }

    iframe();
    begin(); // set the instruction pointer
    while (tick()) {}

    if (return_code == 0 and uregisters[0]) {
        // if return code if the default one and
        // return register is not unused
        // copy value of return register as return code
        try {
            return_code = static_cast<Integer*>(uregisters[0])->value();
        } catch (const char*& e) {
            return_code = 1;
            return_exception = "ReturnStageException";
            return_message = string(e);
        } catch (const string& e) {
            return_code = 1;
            return_exception = "ReturnStageException";
            return_message = string(e);
        }
    }

    // delete entry function's frame
    // otherwise we get huge memory leak
    // do not delete if execution was halted because of exception
    if (return_exception == "") {
        delete frames.back();
    }

    return return_code;
}