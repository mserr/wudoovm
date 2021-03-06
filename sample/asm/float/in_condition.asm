; This script tests whether floats can be freely used as conditions for branch instruction.
; Basically, it justs tests correctness of the .boolean() method override in Integer objects.

.function: main
    fstore 1 0.0001

    ; generate false
    istore 2 0
    istore 3 1
    ieq 2 2 3

    ; check
    branch 1 ok fin
    .mark: ok
    not 2
    .mark: fin
    print 2
    izero 0
    end
.end
