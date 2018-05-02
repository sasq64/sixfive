# sixfive
6502 assembler/emulator/disassembler implemented in modern C++


## Emulator

Implements a basic 6502 processor, including Decimal mode but
excluding illegal opcodes.
Counts instructions cycles but is not cycle correct (yet).

### Example Code

```c++
    Machine<> machine;
    machine.mapRom(0xe0, &kernal[0], 8192);
    machine.mapRom(0xa0, &basic[0], 8192);
    machine.setPC(0x8000);
    machine.run(1000000);
```

### Implementation Details

The actual emulator is contained in a single file, `emulator.h`.
It makes heavy use of templates, constexpr and inlining to make it
readable and consise (very litte redundant code) while still being fast.

Inlining/speed is ensured by an external test that disassembles the
generated (x86) code for each 6502 opcode, and checks that it contains no
calls or jumps, and that the total opcode count stays within reasonable limits

For instance, here is the code generated for ORA. Most of the code is actually
related to updating the 6502 status register.

```Assembly
    ; edi points to the `Machine` object
    movzx eax, word ptr [edi+12]      ; Load PC into eax
    lea ecx, [eax+01]                 ; ecx = eax + 1 (Increment PC)
    movzx eax, byte ptr [edi+eax+14]  ; Load operand into eax
    mov [edi+12], cx                  ; Store incremented PC
    or eax, [edi]                     ; Or A into eax
    mov [edi], eax                    ; Store back A
    ; The rest of the code is just updating the status register:
    mov ecx, FFFFFF7D                 ; Load mask
    and ecx, [edi+0C]                 ; Read SR and mask out bits
    mov edx, eax
    and edx, 80                       ; And 0x80 to A register
    or edx, ecx                       ; OR result to update signed flag
    xor ecx, ecx                      ; 
    test eax, eax
    sete cl                           ; cl := 1 if value is zero
    lea eax, [edx+ecx*2]              ; Update zero flag
    mov [edi+0C], eax                 ; Store back SR
    ret
```

