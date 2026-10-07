// Fixtures for C3 batch c3ai (expandTextMarkup 0x004942f0, buildScenarioIntro 0x0045fd80). Only our own synthetic
// values are written; the two places where the game's own bytes are needed (the elision words at 0x004e4688 and the
// scenario strings the narration is built from) are read out of the running image, never reproduced here.
// Allocations go through Memory.alloc, which diff_hook rewrites to __keepAlloc so blocks referenced only through a
// pointer field survive the run.
(function () {
  function zero(p, size) { for (let i = 0; i < size; i += 4) p.add(i).writeU32(0); }

  // ---------------------------------------------------------------------------------------------------------
  // The markup tables expandTextMarkup reads (all in .data, see shim/src/re/c3ai.cpp for the address of each read).
  //   0x0083de68  int[10]          number slots        ($NUMBER / $NUM / $HEX)
  //   0x0083dda8  int[]            the same array addressed by raw character code ($<#c>): +'0'*4 == 0x0083de68
  //   0x0083e8b8  int[10]          gender per text slot
  //   0x0083d3c8  int[10]          plural per text slot
  //   0x0083e8e0  char[10][0x100]  text slots
  //   0x0083f368  (0x108)[10]      link slots: name at +0, id at +0x104
  //   0x0083fdb8  {ptr, int}[10]   dropdown fall-back lists (record i at base+0x578+i*0x18, key at +8)
  //   0x0083fe08  ptr[10] (8)      dropdown combo objects
  //   0x0083fe5c  int              elision pass enable (1 = on)
  function tables(elision) {
    // char-code indexed number array, codes 0x20..0x7f: a distinct value per code, none of them 1 except where
    // a number slot overwrites it below ($<#c> tests "!= 1").
    for (let c = 0x20; c < 0x80; c++) ptr('0x0083dda8').add(c * 4).writeS32(c * 3 + 1);
    const numbers = [1, 2, 0, 1, 42, -7, 100, 1000, 0, 12345];
    numbers.forEach((v, i) => ptr('0x0083de68').add(i * 4).writeS32(v));
    const genders = [0, 1, 2, 0, 1, 2, 0, 1, 2, 0];
    const plurals = [0, 0, 0, 1, 1, 0, 1, 0, 1, 1];
    for (let i = 0; i < 10; i++) {
      ptr('0x0083e8b8').add(i * 4).writeS32(genders[i]);
      ptr('0x0083d3c8').add(i * 4).writeS32(plurals[i]);
      // text slots: slot 7 is a two-alternative list (one colon), slot 8 is empty, the rest are plain words
      const slot = ptr('0x0083e8e0').add(i * 0x100);
      zero(slot, 0x20);
      if (i === 7) slot.writeUtf8String('c3aiSg:c3aiPl');
      else if (i === 8) slot.writeUtf8String('');
      else slot.writeUtf8String('c3aiT' + i);
      // link slots: inline name at +0, id dword at +0x104
      const link = ptr('0x0083f368').add(i * 0x108);
      zero(link, 0x20);
      link.writeUtf8String('c3aiLink' + i);
      link.add(0x104).writeS32(100 + i * 7);
      // dropdown tables start empty
      ptr('0x0083fdb8').add(i * 8).writePointer(ptr(0));
      ptr('0x0083fdb8').add(i * 8 + 4).writeS32(0);
      ptr('0x0083fe08').add(i * 8).writePointer(ptr(0));
    }

    // Dropdown slot 1: a combo object (0x0083fe08[1] != 0) walked by comboFindItem 0x004940e0. Flags byte +4 bit 4
    // clear selects the +0x2d98 selector and the list base at +0x2e58; [sel+8] = 0, so the base offset is 0x2e58.
    // head = [base+8], count = [base+0x10]; node: +4 id, +8 -> {+4 text pointer}, +0xc next. The key the function
    // looks up is [obj+0x1c].
    const combo = Memory.alloc(0x3000);
    zero(combo, 0x3000);
    const sel = Memory.alloc(0x10);
    zero(sel, 0x10);
    combo.add(0x2d98).writePointer(sel);
    const itemText = Memory.alloc(32); itemText.writeUtf8String('c3aiCombo');
    const item = Memory.alloc(0x10); zero(item, 0x10); item.add(4).writePointer(itemText);
    const node = Memory.alloc(0x20); zero(node, 0x20);
    node.add(4).writeS32(55);                 // node id
    node.add(8).writePointer(item);
    combo.add(0x1c).writeS32(55);             // the key comboFindItem is called with
    combo.add(0x2e58 + 8).writePointer(node); // head
    combo.add(0x2e58 + 0x10).writeS32(1);     // count
    ptr('0x0083fe08').add(1 * 8).writePointer(combo);

    // Dropdown slot 2: no combo, so the fall-back list is searched with findMarkupTokenIndex 0x004a4ad0 (keys at
    // base+0x580, stride 0x18, terminated by -1); key 7 sits in record 0, whose text pointer is at base+0x578.
    const base2 = Memory.alloc(0x800);
    zero(base2, 0x800);
    const t2 = Memory.alloc(32); t2.writeUtf8String('c3aiDrop2');
    base2.add(0x578).writePointer(t2);
    base2.add(0x580).writeS32(7);
    base2.add(0x580 + 0x18).writeS32(-1);     // end of the key list
    ptr('0x0083fdb8').add(2 * 8).writePointer(base2);
    ptr('0x0083fdb8').add(2 * 8 + 4).writeS32(7);

    // Dropdown slot 3: the same shape but the key is absent, so findMarkupTokenIndex returns -1 and the text is
    // read from base+0x578-0x18 = base+0x560 (the index is signed, -1 < 0x100).
    const base3 = Memory.alloc(0x800);
    zero(base3, 0x800);
    const t3 = Memory.alloc(32); t3.writeUtf8String('c3aiDropMiss');
    base3.add(0x560).writePointer(t3);
    base3.add(0x580).writeS32(-1);            // empty key list -> -1
    ptr('0x0083fdb8').add(3 * 8).writePointer(base3);
    ptr('0x0083fdb8').add(3 * 8 + 4).writeS32(5);

    ptr('0x0083fe5c').writeS32(elision ? 1 : 0);
    return combo;
  }

  // One input string per vector, with its own 0x200 output slice of a single block (the block is the state region).
  function strings(list) {
    const outs = Memory.alloc(0x200 * (list.length + 1));
    zero(outs, 0x200 * (list.length + 1));
    const fx = { obj: outs, outs: outs };
    for (let i = 0; i < list.length; i++) {
      const b = Memory.alloc(0x200);
      zero(b, 0x200);
      b.writeUtf8String(list[i]);
      fx['i' + i] = b;
      fx['o' + i] = outs.add(i * 0x200);
    }
    return fx;
  }

  // 43 inputs, one per branch group of expandTextMarkup (the table in log/c3/c3ai_purpose.md says which jcc each
  // one takes). Only our own ASCII; the markup keywords are the grammar the function parses, not game text.
  const MARKUP = [
    /*  0 */ 'plain text with no marker',
    /*  1 */ 'a$$b',
    /*  2 */ 'n=$NUMBER3.',
    /*  3 */ 'n=$NUMBER;',
    /*  4 */ 'n=$NUM1.',
    /*  5 */ 'n=$NUMz',
    /*  6 */ 'h=$HEX2.',
    /*  7 */ 'h=$HEX;',
    /*  8 */ '$NOTATOKEN',
    /*  9 */ '$HI7 tail',
    /* 10 */ '$LINK<keep> rest',
    /* 11 */ 'L=$LINK4.',
    /* 12 */ 'L=$LINKz',
    /* 13 */ '$LOW3 tail',
    /* 14 */ 'D=$DROPDOWN1.',
    /* 15 */ 'D=$DROPLINK2.',
    /* 16 */ 'D=$DROPDOWNz',
    /* 17 */ '$DRAT5 tail',
    /* 18 */ '$ 6 spaced',
    /* 19 */ '$Z0 zed',
    /* 20 */ '$9',
    /* 21 */ 'no digit here $QQ',
    /* 22 */ '$<M:a:b> x',
    /* 23 */ '$<F:a:b:c> x',
    /* 24 */ '$<N:a:b:c> x',
    /* 25 */ '$<m2:a:b> x',
    /* 26 */ '$<f3:a:b:c:d> x',
    /* 27 */ '$<n4:a:b:c:d:e:f> x',
    /* 28 */ '$<M:a:b:c:d:e> x',
    /* 29 */ '$<3:a:b> x',
    /* 30 */ '$<#3:a:b> x',
    /* 31 */ '$<M 7> x',
    /* 32 */ '$<M 8> x',
    /* 33 */ '$<M abc> x',
    /* 34 */ '$<:a:b> x',
    /* 35 */ '$<M:a:b x',
    /* 36 */ '$<#>abc',
    /* 37 */ 'mix $NUM0 and $<F:a:b> and $2 end',
    /* 38 */ '$NUMBER9$NUM8$HEX7',
    /* 39 */ '',
    /* 40 */ 'D=$DROPLINK3.',
    /* 41 */ '$<  F:a:b:c> y',
    /* 42 */ '$<M  7> y',
  ];

  // The elision pass (0x0083fe5c == 1) rewrites the finished output, so these inputs are built around the six
  // words at 0x004e4688 + i*0x14 read out of the image (never spelled out here).
  function elisionInputs() {
    const w = [];
    for (let i = 0; i < 6; i++) w.push(ptr('0x004e4688').add(i * 0x14).readUtf8String());
    return [
      /* 0 */ 'z' + w[0] + 'alpha',               // 4-char form, vowel -> elided
      /* 1 */ 'z' + w[0] + 'beta',                // 4-char form, consonant -> unchanged
      /* 2 */ w[3] + 'Ate now',                   // 3-char form, vowel
      /* 3 */ 'q' + w[1] + 'x' + w[0] + 'elf',    // two different words: the second match is earlier in the table
      /* 4 */ 'nothing to elide',                 // no match at all
      /* 5 */ w[2] + 'oui ' + w[2] + 'ici',       // two occurrences -> the outer loop runs twice
      /* 6 */ 'h test ' + w[4] + 'hotel',         // the 'h' member of the vowel set
      /* 7 */ '$1 ' + w[0] + 'Ile',               // markup expansion and elision in one call
      /* 8 */ w[5] + 'Union',                     // last table entry, uppercase vowel
      /* 9 */ 'x' + w[0] + 'a y' + w[1] + 'b',    // the second match is later: keeps the first best
      /*10 */ 'z' + w[0] + 'usine',               // 'u'
      /*11 */ 'z' + w[0] + 'yeux',                // 'y'
      /*12 */ 'z' + w[1] + 'Hotel',               // 'H'
      /*13 */ 'z' + w[2] + 'Elle',                // 'E'
      /*14 */ w[3] + 'Oui',                       // 'O'
      /*15 */ w[4] + 'Yves',                      // 'Y'
    ];
  }

  // ---------------------------------------------------------------------------------------------------------
  // buildScenarioIntro 0x0045fd80: the clock 0x00834170 (0x20 = the opening narration, 0x1400 = the later one),
  // the suppress flag 0x0059e7b8 bit 0x01000000, the course-type byte 0x005a34e0 (0..3 select a story, anything
  // else only starts the message) and the cash 0x00571fd4. Its two callees need their own preconditions:
  // appendCourseTitle 0x0040daa0 reads the course index 0x0059bf90, the course record 0x00571ff4 (stride 0x2e,
  // site byte at +0), the site name at 0x004c1ea9 + site*0x82, the hole count 0x005685f0 and the string table
  // 0x0059d81c; startMessage 0x0040cb00 reads the direction byte 0x00569498, the busy dword 0x0053df54, the mode
  // 0x00567afc and the difficulty 0x00822c88, and draws two random numbers from the RNG seed 0x00822d9c.
  function intro(clock, kind, suppress, cash) {
    ptr('0x00834170').writeS32(clock);
    ptr('0x0059e7b8').writeU32(suppress ? 0x01000000 : 0);
    ptr('0x005a34e0').writeS8(kind);
    ptr('0x00571fd4').writeS32(cash);
    zero(ptr('0x0051a068'), 0x300);              // the whole state region, so only this call's bytes are compared
    zero(ptr('0x005a6d40'), 0x280);              // startMessage's copy of the text buffer
    zero(ptr('0x00824134'), 32);
    ptr('0x005a7144').writeS32(0);
    // appendCourseTitle
    ptr('0x0059d81c').writeU16(0xffff);          // appendString appends nothing and returns 0
    ptr('0x0059bf90').writeS32(0);               // course 0
    ptr('0x00571ff4').writeU8(0);                // course 0 -> site 0
    ptr('0x004c1ea9').writeUtf8String('c3aiLinks');
    ptr('0x005685f0').writeS32(9);               // hole count (selects the title suffix)
    // startMessage
    ptr('0x00569498').writeU8(0);                // idle: not refused
    ptr('0x0053df54').writeS32(0);
    ptr('0x00567afc').writeS32(0);
    ptr('0x00822c88').writeS32(0);               // difficulty 0
    ptr('0x00822d9c').writeU32(0x12345678);      // RNG seed (a state region, restored per vector)
    return {};
  }

  // showTutorialText 0x004604f0: the page number 0x00567a18 selects the page (jump table 0x00460d70, page-1);
  // 16 pages aim the camera 0x004c2ba0/0x004c2ba4 at tiles read from the three hole corner pairs and from two
  // placed-object words, so all of those are seeded with our own values; startMessage needs the same
  // preconditions as above. Pages 11 and 29 are never used by a vector: their block calls saveGame 0x0040b4a0,
  // which writes a file.
  function tutorial(page) {
    ptr('0x00567a18').writeS32(page);
    zero(ptr('0x0051a068'), 0x300);
    zero(ptr('0x005a6d40'), 0x280);
    ptr('0x004c2ba0').writeS32(0);
    ptr('0x004c2ba4').writeS32(0);
    ptr('0x004c2844').writeS32(0);
    ptr('0x00543d04').writeS32(0);
    ptr('0x005685f4').writeS32(0);
    // hole corner tiles: (x, y) and (x2, y2) for three holes
    const tiles = [['0x00575cc0', 11], ['0x00575cc4', 23], ['0x00575cd0', 37], ['0x00575cd4', 41],
                   ['0x00575ec8', 17], ['0x00575ecc', 29], ['0x00575ed8', 43], ['0x00575edc', 53],
                   ['0x005760d0', 19], ['0x005760d4', 31], ['0x005760e0', 47], ['0x005760e4', 59]];
    tiles.forEach(t => ptr(t[0]).writeS32(t[1]));
    ptr('0x0058bcba').writeS16(13);      // placed object 0: x, y
    ptr('0x0058bcbc').writeS16(27);
    ptr('0x0058bd0a').writeS16(7);       // placed object 5: x, y
    ptr('0x0058bd0c').writeS16(33);
    // startMessage
    ptr('0x00569498').writeU8(0);
    ptr('0x0053df54').writeS32(0);
    ptr('0x00567afc').writeS32(0);
    ptr('0x00822c88').writeS32(0);
    ptr('0x00822d9c').writeU32(0x12345678);
    return {};
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3ai_markup() { const c = tables(false); const fx = strings(MARKUP); fx.combo = c; return fx; },
    c3ai_markup_fr() { const c = tables(true); const fx = strings(elisionInputs()); fx.combo = c; return fx; },

    c3ai_intro_k0() { return intro(0x20, 0, false, 125000); },
    c3ai_intro_k1() { return intro(0x20, 1, false, 7000); },
    c3ai_intro_k2() { return intro(0x20, 2, false, 125000); },
    c3ai_intro_k3() { return intro(0x20, 3, false, 125000); },
    c3ai_intro_kdef() { return intro(0x20, 7, false, 125000); },     // course type out of 0..3: no text is built
    c3ai_intro_kneg() { return intro(0x20, -1, false, 125000); },    // the byte is sign-extended before the switch
    c3ai_intro_flag() { return intro(0x20, 0, true, 125000); },      // suppress flag set: returns at once
    c3ai_intro_late() { return intro(0x1400, 0, false, 125000); },   // the second, fixed message

    // one fixture per tutorial page: 0 (out of range), 1..10, 12 (a page the table drops) and 21..28
    c3ai_tut_p0() { return tutorial(0); },
    c3ai_tut_p1() { return tutorial(1); },
    c3ai_tut_p2() { return tutorial(2); },
    c3ai_tut_p3() { return tutorial(3); },
    c3ai_tut_p4() { return tutorial(4); },
    c3ai_tut_p5() { return tutorial(5); },
    c3ai_tut_p6() { return tutorial(6); },
    c3ai_tut_p7() { return tutorial(7); },
    c3ai_tut_p8() { return tutorial(8); },
    c3ai_tut_p9() { return tutorial(9); },
    c3ai_tut_p10() { return tutorial(10); },
    c3ai_tut_p12() { return tutorial(12); },
    c3ai_tut_p21() { return tutorial(21); },
    c3ai_tut_p22() { return tutorial(22); },
    c3ai_tut_p23() { return tutorial(23); },
    c3ai_tut_p24() { return tutorial(24); },
    c3ai_tut_p25() { return tutorial(25); },
    c3ai_tut_p26() { return tutorial(26); },
    c3ai_tut_p27() { return tutorial(27); },
    c3ai_tut_p28() { return tutorial(28); },
  });
})();
