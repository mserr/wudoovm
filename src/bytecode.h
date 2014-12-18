#ifndef TATANKA_BYTECODES_H
#define TATANKA_BYTECODES_H

#pragma once

enum Bytecode {
    ISTORE,     // ISTORE <register> <number>
    IADD,       // IADD <register-a> <register-b> <register-result> (a + b)
    ISUB,       // ISUB <register-a> <register-b> <register-result> (a - b)
    IMUL,       // IMUL <register-a> <register-b> <register-result> (a * b)
    IDIV,       // IDIV <register-a> <register-b> <register-result> (a / b)

    ILT,        // ILT  <reg-a> <reg-b> <reg-result> (a < b)
    ILTE,       // ILTE <reg-a> <reg-b> <reg-result> (a <= b)
    IGT,        // IGT  <reg-a> <reg-b> <reg-result> (a > b)
    IGTE,       // IGTE <reg-a> <reg-b> <reg-result> (a >= b)
    IEQ,        // IEQ  <reg-a> <reg-b> <reg-result> (a == b)

    PRINT,      // PRINT <register-index>

    BRANCH,     // BRANCH <bytecode-index>
    BRANCHIF,   // BRANCHIF <reg-bool> <bytecode-index-if-true> <bytecode-index-if-false>

    RET,        // RET <reg-index>
    END,        // END  (end, e.g. function call)

    HALT,       // HALT
};

#endif
