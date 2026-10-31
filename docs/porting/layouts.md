# The layout of the compiler's words

A word is 64 bits and a half is 32 (`HL(w)`, `HR(w)`, `HW(l,r)`, `HMASK`: `src/impsyn64.imc`). The tables of the compiler are
laid out for that. Bit numbers are of the 64-bit word.

**Names.** A name is an array of words with eight characters each, the first in the low byte, ended by a NUL (a string
constant is one). `DIR(NM)` returns the directory index of the name starting at `NM`; `DNAME(I,J)` is the Jth word of the
name of entry I. The hash is a fold of the first word to 9 bits.

**The directory** (`PRP`, nodes of three words, `ALF` holds the names):

    word 0   left: the next node of the bucket    right: the index of the name in ALF
    word 1   left: ENTRY                          right: NODE
    word 2   SEM bits 0-31, ATYPE 32-34, COM 35-36, ISNM 37-38, LXTY 39-40, USE 41-43

**Free storage** is one growable array of words at `FB`; an array is named by the index of its first data word and has
a four word header before it: the headers before and after (halves), the name, the increment (left) and length
(right), and 0 for a fixed array or a slot of the registry of relocatable arrays.

**A stack entry** (the code stack and the semantic stack), three words:

    word 0   left: DI, usually a directory index   bits 6-7: AR (1 integer, 2 real, 3 byte)   bits 0-5: TY
    word 1   VAL: the value; for TY 2, 8 and 16: bit 63 indirect, bits 32-42 the register, the right half the offset (signed)
    word 2   left: CB, the first record of its code   right: CE, the last

TY is 0 production, 1 raw identifier, 2 register, 4 constant (VAL is the value), 8 identifier, 16 subscripted variable.

**The semantics array** (`SEMS`): a word has V in bits 0-31, N in bits 32-43 and the type in bits 44-49; for the
types 4, 5 and 10 the 44 bits of N and V are one signed number. Type 0 starts the semantics of a production with N arguments;
bits 41-43 are its priority. `PROSEM` holds, for each production, its levels and the terminal nodes it matches: pairs of
words with a node in the left half and a link in the right.

**The code tree** (`CT`): a node is two or three words named by the index of the first.

    word -1  the first production node among the alternates
    word 0   SCR bits 0-19, ALT bits 20-39, NODEN bits 40-59
    word 1   the type in bits 61-63 (bit 63 is free: ENTRCA marks nodes with it):
               2  match the Kth item (K in the right half)
               3  match identifier ID (right half); bit 32 set: ID is a one-word constant whose value is in word 2
               1  match a stack item of the kind the other bits say: X 60, I 59 (floating point, integer), W S V C R N 58-53
                  (one word of code, subscripted, variable, constant, register, raw identifier), PP 24-31 and QQ 16-23 (the
                  registers allowed), KKK 8-15 and LLL 0-7 (a constant: KKK bits from LLL up)
             W=S=V=C=R=N=0: a production node: its id in the right half, bit 32 a class of VAL/ productions, bit 33 terminal

**The syntax graph** (`GA`): two words a node, the halves SCR and ALT; the second has the type in bits 48-50, NB from
bit 32, the value in the right half. Parser output and the parser's stacks hold a production number or minus a directory
index in the left half and the link in the right.

**Registers** (`RG`, in `impsem`): bits 0-4 the upper limit, 5 two registers, 6-10 the lower limit, 11 a subroutine
call, 12 not a temporary, 13-17 the hardware register. The registers `0R` to `15R` are the indices 33 to 48.

**Code** is a chain of five-word records in `CO` (`docs/porting/llvm-ir-machine.md`).
