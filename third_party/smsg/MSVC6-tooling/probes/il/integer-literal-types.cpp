



int gi;
void z1() { gi = sizeof(2147483647); }
void z2() { gi = sizeof(2147483648); }
void z3() { gi = sizeof(4294967296); }
void z4() { gi = sizeof(0x80000000); }
void z5() { gi = sizeof(0x100000000); }
void u1() { gi = (2147483647 > -1); }
void u2() { gi = (2147483648 > -1); }
void u3() { gi = (4294967296 > -1); }
void u4() { gi = (0x80000000 > -1); }









