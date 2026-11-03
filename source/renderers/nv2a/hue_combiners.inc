// Hardware-validated hue endpoint programs.

static void rgbRotationCombinerApply() {
    constexpr uint32_t zero = 0, factor0 = 1, factor1 = 2;
    constexpr uint32_t diffuse = 4, texture0 = 8, spare0 = 12, spare1 = 13;
    uint32_t *p = pb_begin();
    auto stage = [&](int index, uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                     uint32_t ab, uint32_t cd, uint32_t sum, bool dotAB, bool dotCD,
                     bool oneB = false, bool oneD = false) {
        p = pb_push1(p, NV097_SET_COMBINER_COLOR_ICW + index * 4,
            XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_A_SOURCE, a) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_B_SOURCE, b) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_C_SOURCE, c) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_D_SOURCE, d) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_B_MAP, oneB ? 1 : 0) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_D_MAP, oneD ? 1 : 0));
        p = pb_push1(p, NV097_SET_COMBINER_COLOR_OCW + index * 4,
            XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_AB_DST, ab) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_CD_DST, cd) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_SUM_DST, sum) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_AB_DOT_ENABLE, dotAB) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_CD_DOT_ENABLE, dotCD));
        if (index != 0) {
            // Stage 0 retains texture alpha * vertex alpha from the standard
            // combiner. Later stages discard alpha outputs, preserving diffuse.a.
            p = pb_push1(p, NV097_SET_COMBINER_ALPHA_ICW + index * 4, 0);
            p = pb_push1(p, NV097_SET_COMBINER_ALPHA_OCW + index * 4, 0);
        }
    };
    // Per-stage constants are packed AARRGGBB, as in nxdk's fp20compiler.
    p = pb_push1(p, NV097_SET_COMBINER_FACTOR0 + 0 * 4, 0x0000ff00); // G basis
    p = pb_push1(p, NV097_SET_COMBINER_FACTOR1 + 0 * 4, 0x000000ff); // B basis
    stage(0, texture0, factor0, texture0, factor1, spare0, spare1, zero, true, true);
    // spare0 = (G,0,0) + (0,B,0).
    p = pb_push1(p, NV097_SET_COMBINER_FACTOR0 + 1 * 4, 0x00ff0000);
    p = pb_push1(p, NV097_SET_COMBINER_FACTOR1 + 1 * 4, 0x0000ff00);
    stage(1, spare0, factor0, spare1, factor1, zero, zero, spare0, false, false);
    // Extract R into spare1 while carrying packed G/B in spare0.
    p = pb_push1(p, NV097_SET_COMBINER_FACTOR0 + 2 * 4, 0x00ff0000);
    stage(2, texture0, factor0, spare0, zero, spare1, spare0, zero, true, false, false, true);
    // spare0 = (G,B,0) + (0,0,R).
    p = pb_push1(p, NV097_SET_COMBINER_FACTOR0 + 3 * 4, 0x000000ff);
    stage(3, spare0, zero, spare1, factor0, zero, zero, spare0, false, false, true);
    // Preserve normal vertex RGB modulation; final combiner handles premultiplication.
    stage(4, spare0, diffuse, zero, zero, diffuse, zero, zero, false, false);
    p = pb_push1(p, NV097_SET_COMBINER_CONTROL,
        XGU_MASK(NV097_SET_COMBINER_CONTROL_FACTOR0, NV097_SET_COMBINER_CONTROL_FACTOR0_EACH_STAGE) |
        XGU_MASK(NV097_SET_COMBINER_CONTROL_FACTOR1, NV097_SET_COMBINER_CONTROL_FACTOR1_EACH_STAGE) |
        XGU_MASK(NV097_SET_COMBINER_CONTROL_ITERATION_COUNT, 5));
    pb_end(p);
}

static void hue120CombinerApply() {
    constexpr uint32_t zero = 0, factor0 = 1, factor1 = 2;
    constexpr uint32_t diffuse = 4, texture0 = 8, spare0 = 12, spare1 = 13;
    uint32_t *p = pb_begin();
    auto stage = [&](int index, uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                     uint32_t ab, uint32_t cd, uint32_t sum, bool dotAB, bool dotCD,
                     bool oneB = false, bool oneD = false) {
        p = pb_push1(p, NV097_SET_COMBINER_COLOR_ICW + index * 4,
            XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_A_SOURCE, a) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_B_SOURCE, b) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_C_SOURCE, c) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_D_SOURCE, d) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_B_MAP, oneB ? 1 : 0) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_D_MAP, oneD ? 1 : 0));
        p = pb_push1(p, NV097_SET_COMBINER_COLOR_OCW + index * 4,
            XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_AB_DST, ab) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_CD_DST, cd) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_SUM_DST, sum) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_AB_DOT_ENABLE, dotAB) |
            XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_CD_DOT_ENABLE, dotCD));
        if (index != 0) {
            // Stage 0 retains texture alpha * vertex alpha from the standard
            // combiner. Later stages discard alpha outputs, preserving diffuse.a.
            p = pb_push1(p, NV097_SET_COMBINER_ALPHA_ICW + index * 4, 0);
            p = pb_push1(p, NV097_SET_COMBINER_ALPHA_OCW + index * 4, 0);
        }
    };
    // Per-stage constants are packed AARRGGBB, as in nxdk's fp20compiler.
    p = pb_push1(p, NV097_SET_COMBINER_FACTOR0 + 0 * 4, 0x000000ff); // B basis
    p = pb_push1(p, NV097_SET_COMBINER_FACTOR1 + 0 * 4, 0x00ff0000); // R basis
    stage(0, texture0, factor0, texture0, factor1, spare0, spare1, zero, true, true);
    // spare0 = (B,0,0) + (0,R,0).
    p = pb_push1(p, NV097_SET_COMBINER_FACTOR0 + 1 * 4, 0x00ff0000);
    p = pb_push1(p, NV097_SET_COMBINER_FACTOR1 + 1 * 4, 0x0000ff00);
    stage(1, spare0, factor0, spare1, factor1, zero, zero, spare0, false, false);
    // Extract G into spare1 while carrying packed B/R in spare0.
    p = pb_push1(p, NV097_SET_COMBINER_FACTOR0 + 2 * 4, 0x0000ff00);
    stage(2, texture0, factor0, spare0, zero, spare1, spare0, zero, true, false, false, true);
    // spare0 = (B,R,0) + (0,0,G).
    p = pb_push1(p, NV097_SET_COMBINER_FACTOR0 + 3 * 4, 0x000000ff);
    stage(3, spare0, zero, spare1, factor0, zero, zero, spare0, false, false, true);
    // Preserve normal vertex RGB modulation; final combiner handles premultiplication.
    stage(4, spare0, diffuse, zero, zero, diffuse, zero, zero, false, false);
    p = pb_push1(p, NV097_SET_COMBINER_CONTROL,
        XGU_MASK(NV097_SET_COMBINER_CONTROL_FACTOR0, NV097_SET_COMBINER_CONTROL_FACTOR0_EACH_STAGE) |
        XGU_MASK(NV097_SET_COMBINER_CONTROL_FACTOR1, NV097_SET_COMBINER_CONTROL_FACTOR1_EACH_STAGE) |
        XGU_MASK(NV097_SET_COMBINER_CONTROL_ITERATION_COUNT, 5));
    pb_end(p);
}

static void hue180EndpointCombinerApply() {
    constexpr uint32_t z=0,f0=1,f1=2,v=4,spec=5,t=8,r0=12,r1=13;
    // ICW byte: source[3:0], component selection[4], mapping[7:5].
    auto in=[](uint32_t source,uint32_t map=0,bool alpha=false) {
        return source|(map<<5)|(alpha?16u:0u);
    };
    uint32_t *p=pb_begin();
    auto stage=[&](int i,bool alpha,uint32_t a,uint32_t b,uint32_t c,uint32_t d,
                    uint32_t ab,uint32_t cd,uint32_t sum,bool dotAB=false,bool dotCD=false) {
        p=pb_push1(p,(alpha?NV097_SET_COMBINER_ALPHA_ICW:NV097_SET_COMBINER_COLOR_ICW)+i*4,
            (a<<24)|(b<<16)|(c<<8)|d);
        p=pb_push1(p,(alpha?NV097_SET_COMBINER_ALPHA_OCW:NV097_SET_COMBINER_COLOR_OCW)+i*4,
            (ab<<4)|cd|(sum<<8)|(dotAB?(1u<<13):0)|(dotCD?(1u<<12):0));
    };
    const uint32_t one=in(z,1),negativeOne=in(z,2);
    p=pb_push1(p,NV097_SET_COMBINER_FACTOR0,0x00ff0000);
    p=pb_push1(p,NV097_SET_COMBINER_FACTOR1,0x0000ff00);
    stage(0,false,in(t),in(f0),in(t),in(f1),r0,r1,z,true,true);
    // Two independent alpha products: preserve opacity and save unmodulated B.
    stage(0,true,in(t,6,true),in(v,6,true),in(t),one,v,spec,z);
    stage(1,false,in(r0,6),one,in(r1,6),negativeOne,z,z,r0); // d=R-G
    stage(1,true,in(r0),one,in(r1),one,r0,r1,z); // RGB BLUE -> alpha: R,G
    stage(2,false,in(r1),one,in(r0),one,z,z,r0); // maxRG=G+positive(d)
    stage(2,true,in(r0,0,true),one,in(r0),negativeOne,z,z,r0); // minRG=R-positive(d)
    // Broadcast saved secondary alpha, avoiding a separate RGB B extraction.
    stage(3,false,in(r0,6),one,in(spec,6,true),negativeOne,z,z,r0);
    stage(3,true,in(t),one,in(r0,6,true),negativeOne,z,z,r1);
    stage(4,false,in(spec,0,true),one,in(r0),one,z,z,r0);
    stage(4,true,in(t),one,in(r1,0,true),negativeOne,z,z,r0);
    // Extrema are complete after stage 4; two stages expose/modulate or rotate.

        stage(5,false,in(r0),one,in(t,6),negativeOne,z,z,r0); // V-RGB
        stage(6,false,in(r0),one,in(r0,0,true),one,z,z,r0); // +m
        // Preserve incoming vertex RGB in diffuse. Final E*F modulates R0 by
        // vertex RGB, then A multiplies texture/vertex opacity. G preserves alpha.
        p=pb_push1(p,NV097_SET_COMBINER_SPECULAR_FOG_CW0,
            XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_SOURCE,v)|
            XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_ALPHA,1)|
            XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_SOURCE,
                NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_SOURCE_REG_EF_PROD));
        p=pb_push1(p,NV097_SET_COMBINER_SPECULAR_FOG_CW1,
            XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_E_SOURCE,r0)|
            XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_F_SOURCE,v)|
            XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_SOURCE,v)|
            XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_ALPHA,1));

    stage(5,true,0,0,0,0,z,z,z);
    stage(6,true,0,0,0,0,z,z,z);
    p=pb_push1(p,NV097_SET_COMBINER_CONTROL,
        XGU_MASK(NV097_SET_COMBINER_CONTROL_FACTOR0,NV097_SET_COMBINER_CONTROL_FACTOR0_EACH_STAGE)|
        XGU_MASK(NV097_SET_COMBINER_CONTROL_FACTOR1,NV097_SET_COMBINER_CONTROL_FACTOR1_EACH_STAGE)|
        XGU_MASK(NV097_SET_COMBINER_CONTROL_ITERATION_COUNT,7));
    pb_end(p);
}

// Ordinary HSV +60/+300; shared by production and hardware regression tests.
static void hueEndpointsCombinerApply(int mode) {
    constexpr uint32_t z=0,f0=1,f1=2,v=4,spec=5,t=8,r0=12,r1=13;
    // ICW byte: source[3:0], component selection[4], mapping[7:5].
    auto in=[](uint32_t source,uint32_t map=0,bool alpha=false) {
        return source|(map<<5)|(alpha?16u:0u);
    };
    uint32_t *p=pb_begin();
    auto stage=[&](int i,bool alpha,uint32_t a,uint32_t b,uint32_t c,uint32_t d,
                    uint32_t ab,uint32_t cd,uint32_t sum,bool dotAB=false,bool dotCD=false) {
        p=pb_push1(p,(alpha?NV097_SET_COMBINER_ALPHA_ICW:NV097_SET_COMBINER_COLOR_ICW)+i*4,
            (a<<24)|(b<<16)|(c<<8)|d);
        p=pb_push1(p,(alpha?NV097_SET_COMBINER_ALPHA_OCW:NV097_SET_COMBINER_COLOR_OCW)+i*4,
            (ab<<4)|cd|(sum<<8)|(dotAB?(1u<<13):0)|(dotCD?(1u<<12):0));
    };
    const uint32_t one=in(z,1),negativeOne=in(z,2);
    p=pb_push1(p,NV097_SET_COMBINER_FACTOR0,0x00ff0000);
    p=pb_push1(p,NV097_SET_COMBINER_FACTOR1,0x0000ff00);
    stage(0,false,in(t),in(f0),in(t),in(f1),r0,r1,z,true,true);
    // Two independent alpha products: preserve opacity and save unmodulated B.
    stage(0,true,in(t,6,true),in(v,6,true),in(t),one,v,spec,z);
    stage(1,false,in(r0,6),one,in(r1,6),negativeOne,spec,z,r0); // d=R-G; save AB=R in secondary RGB
    stage(1,true,in(r0),one,in(r1),one,r0,r1,z); // RGB BLUE -> alpha: R,G
    stage(2,false,in(r1),one,in(r0),one,z,z,r0); // maxRG=G+positive(d)
    stage(2,true,in(r0,0,true),one,in(r0),negativeOne,z,z,r0); // minRG=R-positive(d)
    // Broadcast saved secondary alpha, avoiding a separate RGB B extraction.
    stage(3,false,in(r0,6),one,in(spec,6,true),negativeOne,z,z,r0);
    stage(3,true,in(t),one,in(r0,6,true),negativeOne,z,z,r1);
    stage(4,false,in(spec,0,true),one,in(r0),one,z,z,r0);
    stage(4,true,in(t),one,in(r1,0,true),negativeOne,z,z,r0);
    // Independently validated routing and split signed sum, now composed.
    p=pb_push1(p,NV097_SET_COMBINER_FACTOR0+5*4,0x00ff0000);
    p=pb_push1(p,NV097_SET_COMBINER_FACTOR1+5*4,0x0000ff00);
    const bool reverse=mode!=0;
    stage(5,false,reverse?in(spec,0,true):in(r1),in(f0),
        reverse?in(spec):in(spec,0,true),in(f1),z,z,r1);
    stage(5,true,in(r1),one,in(r0,1),negativeOne,spec,r1,z); // save old G; A1=V-1
    p=pb_push1(p,NV097_SET_COMBINER_FACTOR0+6*4,0x000000ff);
    stage(6,false,in(r1),one,in(spec,0,reverse),in(f0),z,z,r1);
    stage(6,true,in(r1,6,true),one,in(r0,0,true),one,z,z,r1); // A1=(V-1)+m
    // Signed A1 plus unsigned inverse of the completed permutation.
    stage(7,false,in(r1,6,true),one,in(r1,1),one,z,z,r0); // V+m-P
    stage(7,true,0,0,0,0,z,z,z);
    p=pb_push1(p,NV097_SET_COMBINER_SPECULAR_FOG_CW0,0x140f0000);
    p=pb_push1(p,NV097_SET_COMBINER_SPECULAR_FOG_CW1,0x0c041400);
    p=pb_push1(p,NV097_SET_COMBINER_CONTROL,
        XGU_MASK(NV097_SET_COMBINER_CONTROL_FACTOR0,NV097_SET_COMBINER_CONTROL_FACTOR0_EACH_STAGE)|
        XGU_MASK(NV097_SET_COMBINER_CONTROL_FACTOR1,NV097_SET_COMBINER_CONTROL_FACTOR1_EACH_STAGE)|
        XGU_MASK(NV097_SET_COMBINER_CONTROL_ITERATION_COUNT,8));
    pb_end(p);
}
