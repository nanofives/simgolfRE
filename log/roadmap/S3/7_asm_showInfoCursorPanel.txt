0x00433190  mov eax, dword ptr [0x55e924]
0x00433195  sub esp, 0x78
0x00433198  test eax, eax
0x0043319a  push ebx
0x0043319b  push ebp
0x0043319c  push esi
0x0043319d  push edi
0x0043319e  je 0x4331c0
0x004331a0  mov eax, dword ptr [esp + 0x90]
0x004331a7  mov ecx, dword ptr [esp + 0x8c]
0x004331ae  push eax
0x004331af  push ecx
0x004331b0  call 0x434cf0
0x004331b5  add esp, 8
0x004331b8  pop edi
0x004331b9  pop esi
0x004331ba  pop ebp
0x004331bb  pop ebx
0x004331bc  add esp, 0x78
0x004331bf  ret 
0x004331c0  xor eax, eax
0x004331c2  movsx edx, byte ptr [eax + 0x4c79a0]
0x004331c9  mov dword ptr [esp + edx*4 + 0x38], eax
0x004331cd  inc eax
0x004331ce  cmp eax, 0x10
0x004331d1  jl 0x4331c2
0x004331d3  movsx ecx, byte ptr [0x5a34e0]
0x004331da  mov eax, 0x4c79ea
0x004331df  lea ecx, [ecx + ecx*2]
0x004331e2  lea ecx, [ecx*8 + 0x4c7a2c]
0x004331e9  mov dx, word ptr [ecx - 4]
0x004331ed  add ecx, 8
0x004331f0  mov word ptr [eax - 2], dx
0x004331f4  mov dx, word ptr [ecx - 8]
0x004331f8  mov word ptr [eax], dx
0x004331fb  add eax, 4
0x004331fe  cmp eax, 0x4c79f2
0x00433203  jle 0x4331e9
0x00433205  mov eax, dword ptr [esp + 0x90]
0x0043320c  mov ecx, dword ptr [esp + 0x8c]
0x00433213  push eax
0x00433214  push ecx
0x00433215  call 0x432f90
0x0043321a  mov ecx, dword ptr [0x4c1570]
0x00433220  add esp, 8
0x00433223  mov ebp, eax
0x00433225  push 0
0x00433227  push 0
0x00433229  push 0
0x0043322b  push 0x519fd8
0x00433230  mov dword ptr [esp + 0x24], ebp
0x00433234  call 0x4762d0
0x00433239  cmp ebp, dword ptr [0x5a9f54]
0x0043323f  jne 0x433699
0x00433245  cmp ebp, -1
0x00433248  je 0x433699
0x0043324e  cmp ebp, 0x14
0x00433251  jge 0x433699
0x00433257  cmp dword ptr [0x5aa554], 0xa
0x0043325e  jle 0x433699
0x00433264  movsx eax, word ptr [ebp*4 + 0x4c79b4]
0x0043326c  add eax, 0x20
0x0043326f  push 0x2d0
0x00433274  push 0x50
0x00433276  push eax
0x00433277  call 0x467130
0x0043327c  mov ebx, eax
0x0043327e  push 1
0x00433280  push 0x70
0x00433282  push 0xa0
0x00433287  lea edx, [ebx - 0x50]
0x0043328a  push 0x192
0x0043328f  push edx
0x00433290  call 0x40cef0
0x00433295  mov ecx, dword ptr [0x4c1570]
0x0043329b  add esp, 0x20
0x0043329e  lea eax, [ebx - 0x5c]
0x004332a1  push 0
0x004332a3  push 0x185
0x004332a8  push eax
0x004332a9  push ecx
0x004332aa  push 0x58b998
0x004332af  mov ecx, 0x5a56e0
0x004332b4  call 0x473f60
0x004332b9  xor esi, esi
0x004332bb  lea edi, [ebx - 0x4c]
0x004332be  xor eax, eax
0x004332c0  cmp esi, 7
0x004332c3  sete al
0x004332c6  inc eax
0x004332c7  mov ecx, dword ptr [0x4c1570]
0x004332cd  push 0
0x004332cf  push 0x185
0x004332d4  lea edx, [eax + eax*4]
0x004332d7  push edi
0x004332d8  push ecx
0x004332d9  lea eax, [eax + edx*2]
0x004332dc  shl eax, 2
0x004332df  lea edx, [eax + 0x58b998]
0x004332e5  lea ecx, [eax + 0x5a56e0]
0x004332eb  push edx
0x004332ec  call 0x473f60
0x004332f1  inc esi
0x004332f2  add edi, 0x10
0x004332f5  cmp esi, 7
0x004332f8  jle 0x4332be
0x004332fa  movsx ebp, byte ptr [ebp + 0x4c79a0]
0x00433301  or ecx, 0xffffffff
0x00433304  xor eax, eax
0x00433306  lea ebp, [ebp + ebp*2]
0x0043330a  push 0xa
0x0043330c  shl ebp, 4
0x0043330f  push 0x5a9f6c
0x00433314  lea edi, [ebp + 0x578350]
0x0043331a  mov dword ptr [esp + 0x18], edi
0x0043331e  repne scasb al, byte ptr es:[edi]
0x00433320  not ecx
0x00433322  sub edi, ecx
0x00433324  mov eax, ecx
0x00433326  mov esi, edi
0x00433328  mov edi, 0x51a068
0x0043332d  shr ecx, 2
0x00433330  rep movsd dword ptr es:[edi], dword ptr [esi]
0x00433332  mov ecx, eax
0x00433334  xor eax, eax
0x00433336  and ecx, 3
0x00433339  rep movsb byte ptr es:[edi], byte ptr [esi]
0x0043333b  mov edi, 0x4c7fdc
0x00433340  or ecx, 0xffffffff
0x00433343  repne scasb al, byte ptr es:[edi]
0x00433345  not ecx
0x00433347  sub edi, ecx
0x00433349  mov esi, edi
0x0043334b  mov edx, ecx
0x0043334d  mov edi, 0x51a068
0x00433352  or ecx, 0xffffffff
0x00433355  repne scasb al, byte ptr es:[edi]
0x00433357  movsx eax, byte ptr [ebp + 0x578373]
0x0043335e  mov ecx, edx
0x00433360  dec edi
0x00433361  shr ecx, 2
0x00433364  rep movsd dword ptr es:[edi], dword ptr [esi]
0x00433366  lea eax, [eax + eax*4]
0x00433369  mov ecx, edx
0x0043336b  and ecx, 3
0x0043336e  lea eax, [eax + eax*4]
0x00433371  shl eax, 2
0x00433374  rep movsb byte ptr es:[edi], byte ptr [esi]
0x00433376  push eax
0x00433377  call 0x4ad425
0x0043337c  mov edi, eax
0x0043337e  or ecx, 0xffffffff
0x00433381  xor eax, eax
0x00433383  add esp, 0xc
0x00433386  repne scasb al, byte ptr es:[edi]
0x00433388  not ecx
0x0043338a  sub edi, ecx
0x0043338c  push 0
0x0043338e  mov esi, edi
0x00433390  mov edx, ecx
0x00433392  mov edi, 0x51a068
0x00433397  or ecx, 0xffffffff
0x0043339a  repne scasb al, byte ptr es:[edi]
0x0043339c  mov ecx, edx
0x0043339e  dec edi
0x0043339f  shr ecx, 2
0x004333a2  rep movsd dword ptr es:[edi], dword ptr [esi]
0x004333a4  mov ecx, edx
0x004333a6  push 0
0x004333a8  and ecx, 3
0x004333ab  push 0
0x004333ad  rep movsb byte ptr es:[edi], byte ptr [esi]
0x004333af  mov al, byte ptr [0x51a068]
0x004333b4  mov ecx, dword ptr [0x4c1570]
0x004333ba  add al, 0xe0
0x004333bc  push 0x51b360
0x004333c1  mov byte ptr [0x51a068], al
0x004333c6  call 0x4762d0
0x004333cb  push 0x80000000
0x004333d0  lea eax, [ebx - 0xc]
0x004333d3  push 0x18a
0x004333d8  push eax
0x004333d9  push 0x51a068
0x004333de  call 0x404b70
0x004333e3  add esp, 0x10
0x004333e6  push 0
0x004333e8  push 0
0x004333ea  mov ecx, dword ptr [0x4c1570]
0x004333f0  push 0
0x004333f2  push 0x519fd8
0x004333f7  call 0x4762d0
0x004333fc  movsx ecx, byte ptr [ebp + 0x578372]
0x00433403  mov esi, dword ptr [0x4c1570]
0x00433409  mov edx, 4
0x0043340e  sub edx, ecx
0x00433410  push 0xa
0x00433412  shl edx, 1
0x00433414  push 1
0x00433416  push edx
0x00433417  call 0x467130
0x0043341c  add esp, 0xc
0x0043341f  lea ecx, [eax + eax*4]
0x00433422  push 0
0x00433424  push 1
0x00433426  lea edx, [eax + ecx*2]
0x00433429  push 1
0x0043342b  push 1
0x0043342d  lea eax, [ebx - 0x5a]
0x00433430  push 0x187
0x00433435  push eax
0x00433436  lea ecx, [edx*4 + 0x59b778]
0x0043343d  push esi
0x0043343e  call 0x473cb0
0x00433443  or ecx, 0xffffffff
0x00433446  mov edi, 0x4c61dc
0x0043344b  xor eax, eax
0x0043344d  repne scasb al, byte ptr es:[edi]
0x0043344f  not ecx
0x00433451  sub edi, ecx
0x00433453  mov edx, ecx
0x00433455  mov esi, edi
0x00433457  mov edi, 0x51a068
0x0043345c  shr ecx, 2
0x0043345f  rep movsd dword ptr es:[edi], dword ptr [esi]
0x00433461  mov ecx, edx
0x00433463  and ecx, 3
0x00433466  rep movsb byte ptr es:[edi], byte ptr [esi]
0x00433468  mov edi, dword ptr [esp + 0x10]
0x0043346c  or ecx, 0xffffffff
0x0043346f  repne scasb al, byte ptr es:[edi]
0x00433471  not ecx
0x00433473  sub edi, ecx
0x00433475  mov esi, edi
0x00433477  mov edx, ecx
0x00433479  mov edi, 0x51a068
0x0043347e  or ecx, 0xffffffff
0x00433481  repne scasb al, byte ptr es:[edi]
0x00433483  mov ecx, edx
0x00433485  dec edi
0x00433486  shr ecx, 2
0x00433489  rep movsd dword ptr es:[edi], dword ptr [esi]
0x0043348b  mov ecx, edx
0x0043348d  lea edx, [esp + 0x18]
0x00433491  and ecx, 3
0x00433494  rep movsb byte ptr es:[edi], byte ptr [esi]
0x00433496  mov edi, 0x51a068
0x0043349b  or ecx, 0xffffffff
0x0043349e  repne scasb al, byte ptr es:[edi]
0x004334a0  not ecx
0x004334a2  sub edi, ecx
0x004334a4  mov eax, ecx
0x004334a6  mov esi, edi
0x004334a8  mov edi, edx
0x004334aa  shr ecx, 2
0x004334ad  rep movsd dword ptr es:[edi], dword ptr [esi]
0x004334af  mov ecx, eax
0x004334b1  movsx eax, byte ptr [0x5a34e0]
0x004334b8  and ecx, 3
0x004334bb  cmp eax, 3
0x004334be  rep movsb byte ptr es:[edi], byte ptr [esi]
0x004334c0  mov byte ptr [0x51a068], 0
0x004334c7  ja 0x433506
0x004334c9  jmp dword ptr [eax*4 + 0x433c14]
0x004334d0  lea ecx, [esp + 0x18]
0x004334d4  push ecx
0x004334d5  push 0x4c7fc4
0x004334da  jmp 0x4334fe
0x004334dc  lea edx, [esp + 0x18]
0x004334e0  push edx
0x004334e1  push 0x4c7fb0
0x004334e6  jmp 0x4334fe
0x004334e8  lea eax, [esp + 0x18]
0x004334ec  push eax
0x004334ed  push 0x4c7f98
0x004334f2  jmp 0x4334fe
0x004334f4  lea ecx, [esp + 0x18]
0x004334f8  push ecx
0x004334f9  push 0x4c7f80
0x004334fe  call 0x45b660
0x00433503  add esp, 8
0x00433506  push 0x80007fff
0x0043350b  lea edx, [ebx - 0x46]
0x0043350e  push 0x19a
0x00433513  push edx
0x00433514  push 0x10
0x00433516  call 0x45b0d0
0x0043351b  mov edi, 0x4c60e0
0x00433520  or ecx, 0xffffffff
0x00433523  xor eax, eax
0x00433525  add esp, 0x10
0x00433528  repne scasb al, byte ptr es:[edi]
0x0043352a  not ecx
0x0043352c  sub edi, ecx
0x0043352e  mov eax, ecx
0x00433530  mov esi, edi
0x00433532  mov edi, 0x51a068
0x00433537  shr ecx, 2
0x0043353a  rep movsd dword ptr es:[edi], dword ptr [esi]
0x0043353c  mov ecx, eax
0x0043353e  movsx eax, byte ptr [ebp + 0x578372]
0x00433545  and ecx, 3
0x00433548  inc eax
0x00433549  cmp eax, 5
0x0043354c  rep movsb byte ptr es:[edi], byte ptr [esi]
0x0043354e  ja 0x43357a
0x00433550  jmp dword ptr [eax*4 + 0x433c24]
0x00433557  mov edi, 0x4c7f78
0x0043355c  jmp 0x43357f
0x0043355e  mov edi, 0x4c7f70
0x00433563  jmp 0x43357f
0x00433565  mov edi, 0x4c7f6c
0x0043356a  jmp 0x43357f
0x0043356c  mov edi, 0x4c7f64
0x00433571  jmp 0x43357f
0x00433573  mov edi, 0x4c7f58
0x00433578  jmp 0x43357f
0x0043357a  mov edi, 0x4c7f4c
0x0043357f  or ecx, 0xffffffff
0x00433582  xor eax, eax
0x00433584  repne scasb al, byte ptr es:[edi]
0x00433586  not ecx
0x00433588  sub edi, ecx
0x0043358a  push 0x80006318
0x0043358f  mov esi, edi
0x00433591  mov edx, ecx
0x00433593  mov edi, 0x51a068
0x00433598  or ecx, 0xffffffff
0x0043359b  repne scasb al, byte ptr es:[edi]
0x0043359d  mov ecx, edx
0x0043359f  dec edi
0x004335a0  shr ecx, 2
0x004335a3  rep movsd dword ptr es:[edi], dword ptr [esi]
0x004335a5  mov ecx, edx
0x004335a7  push 0x1d2
0x004335ac  and ecx, 3
0x004335af  rep movsb byte ptr es:[edi], byte ptr [esi]
0x004335b1  lea esi, [ebx + 0x40]
0x004335b4  mov ecx, 0x519cd4
0x004335b9  push esi
0x004335ba  push 0x1a9
0x004335bf  push esi
0x004335c0  call 0x478b80
0x004335c5  movsx eax, byte ptr [ebp + 0x578370]
0x004335cc  mov ecx, 0x1d2
0x004335d1  push 0x80004210
0x004335d6  lea eax, [eax + eax*2]
0x004335d9  push 0x1d2
0x004335de  shl eax, 1
0x004335e0  sub ecx, eax
0x004335e2  push esi
0x004335e3  push ecx
0x004335e4  push esi
0x004335e5  mov ecx, 0x519cd4
0x004335ea  call 0x478b80
0x004335ef  push 0x80001284
0x004335f4  lea edx, [ebx + 0x44]
0x004335f7  push 0x1d2
0x004335fc  lea esi, [ebx + 0x3c]
0x004335ff  push edx
0x00433600  push 0x1d2
0x00433605  push esi
0x00433606  mov ecx, 0x519cd4
0x0043360b  call 0x478b80
0x00433610  movsx eax, byte ptr [ebp + 0x578370]
0x00433617  mov edx, dword ptr [0x4c1570]
0x0043361d  mov ecx, 0x1cd
0x00433622  lea eax, [eax + eax*2]
0x00433625  push 0
0x00433627  shl eax, 1
0x00433629  sub ecx, eax
0x0043362b  push ecx
0x0043362c  push esi
0x0043362d  push edx
0x0043362e  mov ecx, 0x59b28c
0x00433633  call 0x473e60
0x00433638  mov cl, byte ptr [ebp + 0x578371]
0x0043363e  test cl, cl
0x00433640  je 0x43366b
0x00433642  mov eax, 6
0x00433647  push 0x80004210
0x0043364c  shl eax, cl
0x0043364e  push 0x1ea
0x00433653  lea edx, [ebx - 0x48]
0x00433656  lea ecx, [eax + ebx - 0x48]
0x0043365a  push ecx
0x0043365b  push 0x1ea
0x00433660  push edx
0x00433661  mov ecx, 0x519cd4
0x00433666  call 0x478b80
0x0043366b  mov cl, byte ptr [ebp + 0x578371]
0x00433671  mov eax, 6
0x00433676  mov edx, dword ptr [0x4c1570]
0x0043367c  push 0
0x0043367e  shl eax, cl
0x00433680  push 0x1e2
0x00433685  lea ecx, [eax + ebx - 0x4a]
0x00433689  push ecx
0x0043368a  push edx
0x0043368b  mov ecx, 0x59b28c
0x00433690  call 0x473e60
0x00433695  mov ebp, dword ptr [esp + 0x14]
0x00433699  mov eax, dword ptr [0x4c1570]
0x0043369e  push 0
0x004336a0  push 0x1e2
0x004336a5  push 0xd8
0x004336aa  push eax
0x004336ab  push 0x58b3bc
0x004336b0  mov ecx, 0x541a44
0x004336b5  call 0x473f60
0x004336ba  cmp ebp, 0x15
0x004336bd  jne 0x4336df
0x004336bf  mov ecx, dword ptr [0x4c1570]
0x004336c5  push 0
0x004336c7  push 1
0x004336c9  push 1
0x004336cb  push 1
0x004336cd  push 0x232
0x004336d2  push 0xf6
0x004336d7  push ecx
0x004336d8  mov ecx, 0x571b28
0x004336dd  jmp 0x433702
0x004336df  cmp ebp, 0x16
0x004336e2  jne 0x433707
0x004336e4  mov edx, dword ptr [0x4c1570]
0x004336ea  push 0
0x004336ec  push 1
0x004336ee  push 1
0x004336f0  push 1
0x004336f2  push 0x1fe
0x004336f7  push 0xdc
0x004336fc  push edx
0x004336fd  mov ecx, 0x571bd8
0x00433702  call 0x473cb0
0x00433707  xor esi, esi
0x00433709  xor eax, eax
0x0043370b  cmp ebp, esi
0x0043370d  jne 0x433714
0x0043370f  mov eax, 1
0x00433714  movsx ecx, byte ptr [esi + 0x4c79a0]
0x0043371b  cmp dword ptr [0x4c2848], ecx
0x00433721  jne 0x433728
0x00433723  mov eax, 2
0x00433728  movsx edx, word ptr [esi*4 + 0x4c79b6]
0x00433730  movsx ecx, word ptr [esi*4 + 0x4c79b4]
0x00433738  push 0
0x0043373a  push 1
0x0043373c  push 1
0x0043373e  push 1
0x00433740  lea eax, [eax + esi*4]
0x00433743  push edx
0x00433744  mov edx, dword ptr [0x4c1570]
0x0043374a  push ecx
0x0043374b  lea ecx, [eax + eax*4]
0x0043374e  push edx
0x0043374f  lea edx, [eax + ecx*2]
0x00433752  lea ecx, [edx*4 + 0x570cb8]
0x00433759  call 0x473cb0
0x0043375e  inc esi
0x0043375f  cmp esi, 0x10
0x00433762  jl 0x433709
0x00433764  mov eax, dword ptr [0x5685f0]
0x00433769  mov ecx, eax
0x0043376b  shl ecx, 6
0x0043376e  add ecx, eax
0x00433770  mov edx, dword ptr [ecx*8 + 0x575ab8]
0x00433777  test edx, edx
0x00433779  jne 0x433780
0x0043377b  cmp eax, 0x13
0x0043377e  jl 0x4337ae
0x00433780  movsx edx, word ptr [0x4c79b6]
0x00433787  movsx eax, word ptr [0x4c79b4]
0x0043378e  mov ecx, dword ptr [0x4c1570]
0x00433794  push 0
0x00433796  push 1
0x00433798  push 1
0x0043379a  push 1
0x0043379c  push edx
0x0043379d  push eax
0x0043379e  push ecx
0x0043379f  mov ecx, 0x570d3c
0x004337a4  call 0x473cb0
0x004337a9  mov eax, dword ptr [0x5685f0]
0x004337ae  mov edx, eax
0x004337b0  shl edx, 6
0x004337b3  add edx, eax
0x004337b5  mov ecx, dword ptr [edx*8 + 0x575ac8]
0x004337bc  test ecx, ecx
0x004337be  jne 0x433828
0x004337c0  cmp eax, 0x13
0x004337c3  jge 0x433828
0x004337c5  mov eax, dword ptr [0x4c2848]
0x004337ca  mov cl, byte ptr [0x5a34e0]
0x004337d0  cmp eax, 1
0x004337d3  jne 0x4337e7
0x004337d5  mov eax, dword ptr [0x834170]
0x004337da  movsx esi, cl
0x004337dd  cdq 
0x004337de  idiv dword ptr [esi*4 + 0x53fa0c]
0x004337e5  jmp 0x4337e9
0x004337e7  xor edx, edx
0x004337e9  movsx eax, cl
0x004337ec  push 3
0x004337ee  add eax, 0x189
0x004337f3  push edx
0x004337f4  push eax
0x004337f5  call 0x43d6f0
0x004337fa  movsx ecx, word ptr [0x4c79ba]
0x00433801  movsx edx, word ptr [0x4c79b8]
0x00433808  push 0x63
0x0043380a  push 4
0x0043380c  push eax
0x0043380d  push 0x28
0x0043380f  push 0x28
0x00433811  add ecx, 0x18
0x00433814  push 0
0x00433816  add edx, 0x1f
0x00433819  push ecx
0x0043381a  push edx
0x0043381b  mov dword ptr [0x5aa6c4], eax
0x00433820  call 0x4628d0
0x00433825  add esp, 0x2c
0x00433828  mov ecx, dword ptr [0x4c1570]
0x0043382e  push 0
0x00433830  push 0
0x00433832  push 0
0x00433834  push 0x519fd8
0x00433839  call 0x4762d0
0x0043383e  mov dword ptr [esp + 0x10], 0
0x00433846  mov ebx, 0x4c79b4
0x0043384b  mov eax, dword ptr [esp + 0x10]
0x0043384f  or ecx, 0xffffffff
0x00433852  movsx edx, byte ptr [eax + 0x4c79a0]
0x00433859  xor eax, eax
0x0043385b  lea edi, [edx + edx*2]
0x0043385e  shl edi, 4
0x00433861  add edi, 0x578350
0x00433867  repne scasb al, byte ptr es:[edi]
0x00433869  not ecx
0x0043386b  sub edi, ecx
0x0043386d  mov eax, ecx
0x0043386f  mov esi, edi
0x00433871  mov edi, 0x51a068
0x00433876  shr ecx, 2
0x00433879  rep movsd dword ptr es:[edi], dword ptr [esi]
0x0043387b  mov ecx, eax
0x0043387d  mov eax, dword ptr [0x822c88]
0x00433882  and ecx, 3
0x00433885  rep movsb byte ptr es:[edi], byte ptr [esi]
0x00433887  mov cl, byte ptr [0x51a068]
0x0043388d  add cl, 0xe0
0x00433890  test eax, eax
0x00433892  mov byte ptr [0x51a068], cl
0x00433898  je 0x4338a2
0x0043389a  cmp dword ptr [0x4c2848], edx
0x004338a0  jne 0x4338c3
0x004338a2  movsx ecx, word ptr [ebx + 2]
0x004338a6  movsx edx, word ptr [ebx]
0x004338a9  add ecx, 4
0x004338ac  push 0x80007fff
0x004338b1  add edx, 0x20
0x004338b4  push ecx
0x004338b5  push edx
0x004338b6  push 0x51a068
0x004338bb  call 0x404b70
0x004338c0  add esp, 0x10
0x004338c3  mov edx, dword ptr [esp + 0x10]
0x004338c7  add ebx, 4
0x004338ca  inc edx
0x004338cb  cmp ebx, 0x4c79f0
0x004338d1  mov dword ptr [esp + 0x10], edx
0x004338d5  jle 0x43384b
0x004338db  cmp ebp, dword ptr [0x5a9f54]
0x004338e1  jne 0x433bfc
0x004338e7  mov ecx, dword ptr [0x5aa554]
0x004338ed  inc ecx
0x004338ee  cmp ebp, 0x14
0x004338f1  mov dword ptr [0x5aa554], ecx
0x004338f7  jl 0x433c0c
0x004338fd  mov eax, ecx
0x004338ff  cmp eax, 0xa
0x00433902  jle 0x433c0c
0x00433908  movsx ebx, byte ptr [ebp + 0x4c79a0]
0x0043390f  or ecx, 0xffffffff
0x00433912  xor eax, eax
0x00433914  lea ebx, [ebx + ebx*2]
0x00433917  push 0xa
0x00433919  shl ebx, 4
0x0043391c  push 0x5a9f6c
0x00433921  lea edi, [ebx + 0x578350]
0x00433927  repne scasb al, byte ptr es:[edi]
0x00433929  not ecx
0x0043392b  sub edi, ecx
0x0043392d  mov eax, ecx
0x0043392f  mov esi, edi
0x00433931  mov edi, 0x51a068
0x00433936  shr ecx, 2
0x00433939  rep movsd dword ptr es:[edi], dword ptr [esi]
0x0043393b  mov ecx, eax
0x0043393d  and ecx, 3
0x00433940  rep movsb byte ptr es:[edi], byte ptr [esi]
0x00433942  mov al, byte ptr [0x51a068]
0x00433947  mov edi, 0x4c7f40
0x0043394c  add al, 0xe0
0x0043394e  or ecx, 0xffffffff
0x00433951  mov byte ptr [0x51a068], al
0x00433956  xor eax, eax
0x00433958  repne scasb al, byte ptr es:[edi]
0x0043395a  not ecx
0x0043395c  sub edi, ecx
0x0043395e  mov esi, edi
0x00433960  mov edx, ecx
0x00433962  mov edi, 0x51a068
0x00433967  or ecx, 0xffffffff
0x0043396a  repne scasb al, byte ptr es:[edi]
0x0043396c  movsx eax, byte ptr [ebx + 0x578373]
0x00433973  mov ecx, edx
0x00433975  dec edi
0x00433976  shr ecx, 2
0x00433979  rep movsd dword ptr es:[edi], dword ptr [esi]
0x0043397b  lea eax, [eax + eax*4]
0x0043397e  mov ecx, edx
0x00433980  and ecx, 3
0x00433983  lea eax, [eax + eax*4]
0x00433986  shl eax, 2
0x00433989  rep movsb byte ptr es:[edi], byte ptr [esi]
0x0043398b  push eax
0x0043398c  call 0x4ad425
0x00433991  mov edi, eax
0x00433993  or ecx, 0xffffffff
0x00433996  xor eax, eax
0x00433998  add esp, 0xc
0x0043399b  repne scasb al, byte ptr es:[edi]
0x0043399d  not ecx
0x0043399f  sub edi, ecx
0x004339a1  mov esi, edi
0x004339a3  mov edx, ecx
0x004339a5  mov edi, 0x51a068
0x004339aa  or ecx, 0xffffffff
0x004339ad  repne scasb al, byte ptr es:[edi]
0x004339af  mov ecx, edx
0x004339b1  dec edi
0x004339b2  shr ecx, 2
0x004339b5  rep movsd dword ptr es:[edi], dword ptr [esi]
0x004339b7  mov ecx, edx
0x004339b9  and ecx, 3
0x004339bc  cmp ebp, 0x14
0x004339bf  rep movsb byte ptr es:[edi], byte ptr [esi]
0x004339c1  jne 0x4339ca
0x004339c3  mov edi, 0x4c7f34
0x004339c8  jmp 0x4339fb
0x004339ca  cmp ebp, 0x15
0x004339cd  jne 0x4339f1
0x004339cf  mov edi, 0x4c7f2c
0x004339d4  or ecx, 0xffffffff
0x004339d7  xor eax, eax
0x004339d9  repne scasb al, byte ptr es:[edi]
0x004339db  not ecx
0x004339dd  sub edi, ecx
0x004339df  mov edx, ecx
0x004339e1  mov esi, edi
0x004339e3  mov edi, 0x51a068
0x004339e8  shr ecx, 2
0x004339eb  rep movsd dword ptr es:[edi], dword ptr [esi]
0x004339ed  mov ecx, edx
0x004339ef  jmp 0x433a16
0x004339f1  cmp ebp, 0x16
0x004339f4  jne 0x433a1b
0x004339f6  mov edi, 0x4c7f1c
0x004339fb  or ecx, 0xffffffff
0x004339fe  xor eax, eax
0x00433a00  repne scasb al, byte ptr es:[edi]
0x00433a02  not ecx
0x00433a04  sub edi, ecx
0x00433a06  mov eax, ecx
0x00433a08  mov esi, edi
0x00433a0a  mov edi, 0x51a068
0x00433a0f  shr ecx, 2
0x00433a12  rep movsd dword ptr es:[edi], dword ptr [esi]
0x00433a14  mov ecx, eax
0x00433a16  and ecx, 3
0x00433a19  rep movsb byte ptr es:[edi], byte ptr [esi]
0x00433a1b  mov edi, 0x51a068
0x00433a20  or ecx, 0xffffffff
0x00433a23  xor eax, eax
0x00433a25  lea esi, [ebp*4 + 0x4c79b6]
0x00433a2c  repne scasb al, byte ptr es:[edi]
0x00433a2e  movsx eax, word ptr [ebp*4 + 0x4c79b4]
0x00433a36  movsx edx, word ptr [esi]
0x00433a39  lea edi, [ebp*4 + 0x4c79b4]
0x00433a40  add eax, 0x14
0x00433a43  not ecx
0x00433a45  dec ecx
0x00433a46  push 5
0x00433a48  inc edx
0x00433a49  push 0xa
0x00433a4b  lea ecx, [ecx + ecx*2]
0x00433a4e  push 0x80000000
0x00433a53  push edx
0x00433a54  lea ebp, [ecx + eax]
0x00433a57  sub eax, ecx
0x00433a59  push ebp
0x00433a5a  push edx
0x00433a5b  push eax
0x00433a5c  call 0x4493d0
0x00433a61  movsx ecx, word ptr [esi]
0x00433a64  movsx edx, word ptr [edi]
0x00433a67  sub ecx, 4
0x00433a6a  push 0x80007fff
0x00433a6f  add edx, 0x14
0x00433a72  push ecx
0x00433a73  push edx
0x00433a74  push 0x51a068
0x00433a79  call 0x404b70
0x00433a7e  mov edi, 0x4c60e0
0x00433a83  or ecx, 0xffffffff
0x00433a86  xor eax, eax
0x00433a88  add esp, 0x2c
0x00433a8b  repne scasb al, byte ptr es:[edi]
0x00433a8d  not ecx
0x00433a8f  sub edi, ecx
0x00433a91  mov eax, ecx
0x00433a93  mov esi, edi
0x00433a95  mov edi, 0x51a068
0x00433a9a  shr ecx, 2
0x00433a9d  rep movsd dword ptr es:[edi], dword ptr [esi]
0x00433a9f  mov ecx, eax
0x00433aa1  movsx eax, byte ptr [ebx + 0x578372]
0x00433aa8  and ecx, 3
0x00433aab  inc eax
0x00433aac  cmp eax, 5
0x00433aaf  rep movsb byte ptr es:[edi], byte ptr [esi]
0x00433ab1  ja 0x433bc7
0x00433ab7  jmp dword ptr [eax*4 + 0x433c3c]
0x00433abe  mov edi, 0x4c7f78
0x00433ac3  or ecx, 0xffffffff
0x00433ac6  xor eax, eax
0x00433ac8  repne scasb al, byte ptr es:[edi]
0x00433aca  not ecx
0x00433acc  sub edi, ecx
0x00433ace  mov esi, edi
0x00433ad0  mov edx, ecx
0x00433ad2  mov edi, 0x51a068
0x00433ad7  or ecx, 0xffffffff
0x00433ada  repne scasb al, byte ptr es:[edi]
0x00433adc  mov ecx, edx
0x00433ade  dec edi
0x00433adf  shr ecx, 2
0x00433ae2  rep movsd dword ptr es:[edi], dword ptr [esi]
0x00433ae4  mov ecx, edx
0x00433ae6  and ecx, 3
0x00433ae9  rep movsb byte ptr es:[edi], byte ptr [esi]
0x00433aeb  pop edi
0x00433aec  pop esi
0x00433aed  pop ebp
0x00433aee  pop ebx
0x00433aef  add esp, 0x78
0x00433af2  ret 
0x00433af3  mov edi, 0x4c7f70
0x00433af8  or ecx, 0xffffffff
0x00433afb  xor eax, eax
0x00433afd  repne scasb al, byte ptr es:[edi]
0x00433aff  not ecx
0x00433b01  sub edi, ecx
0x00433b03  mov esi, edi
0x00433b05  mov edx, ecx
0x00433b07  mov edi, 0x51a068
0x00433b0c  or ecx, 0xffffffff
0x00433b0f  repne scasb al, byte ptr es:[edi]
0x00433b11  mov ecx, edx
0x00433b13  dec edi
0x00433b14  shr ecx, 2
0x00433b17  rep movsd dword ptr es:[edi], dword ptr [esi]
0x00433b19  mov ecx, edx
0x00433b1b  and ecx, 3
0x00433b1e  rep movsb byte ptr es:[edi], byte ptr [esi]
0x00433b20  pop edi
0x00433b21  pop esi
0x00433b22  pop ebp
0x00433b23  pop ebx
0x00433b24  add esp, 0x78
0x00433b27  ret 
0x00433b28  mov edi, 0x4c7f6c
0x00433b2d  or ecx, 0xffffffff
0x00433b30  xor eax, eax
0x00433b32  repne scasb al, byte ptr es:[edi]
0x00433b34  not ecx
0x00433b36  sub edi, ecx
0x00433b38  mov esi, edi
0x00433b3a  mov edx, ecx
0x00433b3c  mov edi, 0x51a068
0x00433b41  or ecx, 0xffffffff
0x00433b44  repne scasb al, byte ptr es:[edi]
0x00433b46  mov ecx, edx
0x00433b48  dec edi
0x00433b49  shr ecx, 2
0x00433b4c  rep movsd dword ptr es:[edi], dword ptr [esi]
0x00433b4e  mov ecx, edx
0x00433b50  and ecx, 3
0x00433b53  rep movsb byte ptr es:[edi], byte ptr [esi]
0x00433b55  pop edi
0x00433b56  pop esi
0x00433b57  pop ebp
0x00433b58  pop ebx
0x00433b59  add esp, 0x78
0x00433b5c  ret 
0x00433b5d  mov edi, 0x4c7f64
0x00433b62  or ecx, 0xffffffff
0x00433b65  xor eax, eax
0x00433b67  repne scasb al, byte ptr es:[edi]
0x00433b69  not ecx
0x00433b6b  sub edi, ecx
0x00433b6d  mov esi, edi
0x00433b6f  mov edx, ecx
0x00433b71  mov edi, 0x51a068
0x00433b76  or ecx, 0xffffffff
0x00433b79  repne scasb al, byte ptr es:[edi]
0x00433b7b  mov ecx, edx
0x00433b7d  dec edi
0x00433b7e  shr ecx, 2
0x00433b81  rep movsd dword ptr es:[edi], dword ptr [esi]
0x00433b83  mov ecx, edx
0x00433b85  and ecx, 3
0x00433b88  rep movsb byte ptr es:[edi], byte ptr [esi]
0x00433b8a  pop edi
0x00433b8b  pop esi
0x00433b8c  pop ebp
0x00433b8d  pop ebx
0x00433b8e  add esp, 0x78
0x00433b91  ret 
0x00433b92  mov edi, 0x4c7f58
0x00433b97  or ecx, 0xffffffff
0x00433b9a  xor eax, eax
0x00433b9c  repne scasb al, byte ptr es:[edi]
0x00433b9e  not ecx
0x00433ba0  sub edi, ecx
0x00433ba2  mov esi, edi
0x00433ba4  mov edx, ecx
0x00433ba6  mov edi, 0x51a068
0x00433bab  or ecx, 0xffffffff
0x00433bae  repne scasb al, byte ptr es:[edi]
0x00433bb0  mov ecx, edx
0x00433bb2  dec edi
0x00433bb3  shr ecx, 2
0x00433bb6  rep movsd dword ptr es:[edi], dword ptr [esi]
0x00433bb8  mov ecx, edx
0x00433bba  and ecx, 3
0x00433bbd  rep movsb byte ptr es:[edi], byte ptr [esi]
0x00433bbf  pop edi
0x00433bc0  pop esi
0x00433bc1  pop ebp
0x00433bc2  pop ebx
0x00433bc3  add esp, 0x78
0x00433bc6  ret 
0x00433bc7  mov edi, 0x4c7f4c
0x00433bcc  or ecx, 0xffffffff
0x00433bcf  xor eax, eax
0x00433bd1  repne scasb al, byte ptr es:[edi]
0x00433bd3  not ecx
0x00433bd5  sub edi, ecx
0x00433bd7  mov esi, edi
0x00433bd9  mov edx, ecx
0x00433bdb  mov edi, 0x51a068
0x00433be0  or ecx, 0xffffffff
0x00433be3  repne scasb al, byte ptr es:[edi]
0x00433be5  mov ecx, edx
0x00433be7  dec edi
0x00433be8  shr ecx, 2
0x00433beb  rep movsd dword ptr es:[edi], dword ptr [esi]
0x00433bed  mov ecx, edx
0x00433bef  and ecx, 3
0x00433bf2  rep movsb byte ptr es:[edi], byte ptr [esi]
0x00433bf4  pop edi
0x00433bf5  pop esi
0x00433bf6  pop ebp
0x00433bf7  pop ebx
0x00433bf8  add esp, 0x78
0x00433bfb  ret 
0x00433bfc  mov dword ptr [0x5aa554], 0
0x00433c06  mov dword ptr [0x5a9f54], ebp
0x00433c0c  pop edi
0x00433c0d  pop esi
0x00433c0e  pop ebp
0x00433c0f  pop ebx
0x00433c10  add esp, 0x78
0x00433c13  ret 
