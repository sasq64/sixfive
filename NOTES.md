
Benchmarks

Normal SR:

sort: 135us
allops: 660ns

SR result optimization:

sort: 127us
allops 585ns


    template <int FLAG, bool ON> static constexpr void Branch(Machine& m)
    {
        int8_t diff = m.ReadPC();
        auto d = m.check<FLAG, ON>();
        m.cycles += d;
        m.pc += (diff * d);
    }

bne (11/0/0)
    mov eax, [edi]
    xor ecx, ecx
    xor edx, edx
    cmp byte ptr [edi+14], 00
    setne dl
    movsx esi, byte ptr [edi+eax+19]
    cmove esi, ecx
    add [edi+12028], edx
    lea eax, [esi+eax]
    add eax, 01
    mov [edi], eax
    ret

    template <int FLAG, bool ON> static constexpr void Branch(Machine& m)
    {
        int8_t diff = m.ReadPC();
        if(m.check<FLAG, ON>()) {
            m.cycles++;
            m.pc += diff;
        }
    }

bne (9/0/1)
    mov ecx, [edi]
    lea eax, [ecx+01]
    mov [edi], eax
    cmp byte ptr [edi+14], 00
    je short .skip
    movsx ecx, byte ptr [edi+ecx+19]
    add dword ptr [edi+12028], 01
    add eax, ecx
    mov [edi], eax
.skip
    ret
