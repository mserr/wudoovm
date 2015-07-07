#include "../../types/integer.h"
#include "../../support/pointer.h"
#include "../../exceptions.h"
#include "../cpu.h"
using namespace std;


byte* CPU::tryframe(byte* addr) {
    /** Create new special frame for try blocks.
     */
    if (try_frame_new != 0) {
        throw "new block frame requested while last one is unused";
    }
    try_frame_new = new TryFrame();
    return addr;
}

byte* CPU::vmcatch(byte* addr) {
    /** Run catch instruction.
     */
    string type_name = string(addr);
    addr += (type_name.size()+1);

    string catcher_block_name = string(addr);
    addr += (catcher_block_name.size()+1);

    bool block_found = (block_addresses.count(catcher_block_name) or linked_blocks.count(catcher_block_name));
    if (not block_found) {
        throw new Exception("registering undefined handler block: " + catcher_block_name);
    }

    byte* block_address = 0;
    if (block_addresses.count(catcher_block_name)) {
        block_address = bytecode+block_addresses.at(catcher_block_name);
        jump_base = bytecode;
    } else {
        block_address = linked_blocks.at(catcher_block_name).second;
        jump_base = linked_modules.at(linked_blocks.at(catcher_block_name).first).second;
    }

    try_frame_new->catchers[type_name] = new Catcher(type_name, catcher_block_name, block_address);

    return addr;
}

byte* CPU::pull(byte* addr) {
    /** Run pull instruction.
     */
    int destination_register_index;
    bool destination_register_ref = false;

    destination_register_ref = *((bool*)addr);
    pointer::inc<bool, byte>(addr);
    destination_register_index = *((int*)addr);
    pointer::inc<int, byte>(addr);

    if (destination_register_ref) {
        destination_register_index = static_cast<Integer*>(fetch(destination_register_index))->value();
    }

    if (caught == 0) {
        throw new Exception("no caught object to pull");
    }
    uregset->set(destination_register_index, caught);
    caught = 0;

    return addr;
}

byte* CPU::vmtry(byte* addr) {
    /*  Run try instruction.
     */
    string block_name = string(addr);

    bool block_found = (block_addresses.count(block_name) or linked_blocks.count(block_name));
    if (not block_found) {
        throw new Exception("try of undefined block: " + block_name);
    }

    byte* block_address = 0;
    if (block_addresses.count(block_name)) {
        block_address = bytecode+block_addresses.at(block_name);
        jump_base = bytecode;
    } else {
        block_address = linked_blocks.at(block_name).second;
        jump_base = linked_modules.at(linked_blocks.at(block_name).first).second;
    }

    try_frame_new->return_address = (addr+block_name.size());
    try_frame_new->associated_frame = frames.back();
    try_frame_new->block_name = block_name;

    tryframes.push_back(try_frame_new);
    try_frame_new = 0;

    return block_address;
}

byte* CPU::vmthrow(byte* addr) {
    /** Run throw instruction.
     */
    int source_register_index;
    bool source_register_ref = false;

    source_register_ref = *((bool*)addr);
    pointer::inc<bool, byte>(addr);
    source_register_index = *((int*)addr);
    pointer::inc<int, byte>(addr);

    if (source_register_ref) {
        source_register_index = static_cast<Integer*>(fetch(source_register_index))->value();
    }

    if (unsigned(source_register_index) >= uregset->size()) {
        ostringstream oss;
        oss << "invalid read: register out of bounds: " <<source_register_index;
        throw new Exception(oss.str());
    }
    if (uregset->at(source_register_index) == 0) {
        ostringstream oss;
        oss << "invalid throw: register " << source_register_index << " is empty";
        throw new Exception(oss.str());
    }

    uregset->setmask(source_register_index, KEEP);  // set correct mask
    thrown = uregset->get(source_register_index);

    return addr;
}

byte* CPU::leave(byte* addr) {
    /*  Run leave instruction.
     */
    if (tryframes.size() == 0) {
        throw Exception("bad leave: no block has been entered");
    }
    addr = tryframes.back()->return_address;
    delete tryframes.back();
    tryframes.pop_back();

    if (frames.size() > 0) {
        if (function_addresses.count(frames.back()->function_name)) {
            jump_base = bytecode;
        } else {
            jump_base = linked_modules.at(linked_functions.at(frames.back()->function_name).first).second;
        }
    }
    return addr;
}
