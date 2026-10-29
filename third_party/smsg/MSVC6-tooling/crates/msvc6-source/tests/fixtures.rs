#[cfg(test)]
mod t {
    use msvc6_source::lex::{self, Cond, Kind};
    use msvc6_source::linepos::{self, MacroSet};
    use msvc6_source::strip::{Layout, Plan};
    use msvc6_source::syntax::Syntax;
    use msvc6_source::{defs, renamecheck};

    fn cp1252_source() -> Vec<u8> {
        let mut s = b"static const char k_all[] = \"".to_vec();
        for b in 0x80u8..=0xff {
            if b == b'"' || b == b'\\' {
                continue;
            }
            s.push(b);
        }
        s.extend_from_slice(b"\";\r\n");
        s.extend_from_slice(b"/* \x93quoted\x94 \xa7 note */\r\n");
        s.extend_from_slice(b"char g_sect = '\xa7'; // \xe9t\xe9\r\n");
        s.extend_from_slice(b"int f(int a) { return a; } /* \xff */\r\n");
        s
    }

    fn trigraph_source() -> Vec<u8> {
        b"??=include \"a.h\"\n\
          char g_quote = '??/'';\n\
          char *g_s = \"x??/\"y\"; // after the escaped quote\n\
          char *g_esc = \"\\??=\"; /* a trigraph at an escape */\n\
          int g_arr??(2??) = ??< 1, 2 ??>;\n\
          // a comment ending in a trigraph splice ??/\n\
          int g_spliced_away;\n\
          int g_kept;\n"
            .to_vec()
    }

    fn splice_source() -> Vec<u8> {
        b"#define PAIR(a, b) \\\n    ((a) + (b))\n\
          int g_x = 1; // runs on \\\n\
          int g_hidden;\n\
          char *g_s = \"ab\\\r\ncd\";\n\
          int g_long\\\n_name = PAIR(1, 2);\n"
            .to_vec()
    }

    fn line_source() -> Vec<u8> {
        b"#if 0\nprose: don't \"read\" this; assert(never);\n#line 900\n#endif\n\
          /* a comment above */\n\
          void f(int n)\n{\n    assert(n !=\n           0);\n    g(__LINE__);\n}\n\
          #line 50 \"x.cpp\"\n\
          /* below the pin */\n\
          int h() { return __LINE__; }\n"
            .to_vec()
    }

    fn strip_clean(src: &[u8], layout: Layout) -> Vec<u8> {
        let m = MacroSet::builtin();
        let p = Plan::new(Syntax::C, src, &[], &m);
        let out = p.apply(src, layout);
        p.check(src, &out, layout, &m).unwrap();
        assert_eq!(Plan::new(Syntax::C, &out, &[], &m).stripped(), 0);
        out
    }

    #[test]
    fn cp1252_bytes_pass_through_lexer_and_strip_byte_for_byte() {
        let src = cp1252_source();
        let lx = lex::lex(&src);
        assert!(lx.problems.is_empty(), "{:?}", lx.problems);
        assert_eq!(lx.comments.len(), 3);
        let s = lx.tokens.iter().find(|t| t.kind == Kind::Str).unwrap();
        assert_eq!(
            msvc6_base::srcio::encode(&s.text).unwrap(),
            &src[s.start..s.end]
        );
        let out = strip_clean(&src, Layout::Default);
        let mut want = src.clone();
        for c in lx.comments.iter().rev() {
            let mut start = c.start;
            while start > 0 && src[start - 1] == b' ' {
                start -= 1;
            }
            want.drain(start..c.end);
        }
        let want: Vec<u8> = want
            .split_inclusive(|&b| b == b'\n')
            .filter(|l| *l != b"\r\n")
            .flatten()
            .copied()
            .collect();
        assert_eq!(out, want);
        for b in 0x80u8..=0xff {
            if b != b'"' && b != b'\\' && !matches!(b, 0x93 | 0x94 | 0xe9 | 0xff) {
                assert!(out.contains(&b), "byte {b:#x} lost");
            }
        }
    }

    #[test]
    fn trigraphs_are_replaced_first_in_code_literals_and_comments() {
        let src = trigraph_source();
        let lx = lex::lex(&src);
        assert_eq!(
            lx.directives.first().map(|d| d.name.as_str()),
            Some("include")
        );
        let quote = lx.tokens.iter().find(|t| t.kind == Kind::Chr).unwrap();
        assert_eq!(quote.text, "'\\''");
        let s = lx
            .tokens
            .iter()
            .find(|t| t.kind == Kind::Str && t.line == 3)
            .unwrap();
        assert_eq!(s.text, "\"x\\\"y\"");
        let esc = lx
            .tokens
            .iter()
            .find(|t| t.kind == Kind::Str && t.line == 4)
            .unwrap();
        assert_eq!(esc.text, "\"\\#\"");
        assert!(lx.tokens.iter().any(|t| t.text == "[") && lx.tokens.iter().any(|t| t.text == "{"));
        assert!(!lx.tokens.iter().any(|t| t.text == "g_spliced_away"));
        assert!(lx.tokens.iter().any(|t| t.text == "g_kept"));
        assert_eq!(lx.comments.len(), 3);
        assert!(lx.problems.is_empty(), "{:?}", lx.problems);
        let out = strip_clean(&src, Layout::Default);
        let text = msvc6_base::srcio::decode(&out);
        assert!(text.contains("char g_quote = '??/'';\n"));
        assert!(text.contains("char *g_s = \"x??/\"y\";\n"));
        assert!(text.contains("char *g_esc = \"\\??=\";\n"));
        assert!(!text.contains("g_spliced_away"));
        assert!(text.ends_with("int g_kept;\n"));
    }

    #[test]
    fn splices_join_directives_literals_and_comments_and_keep_physical_lines() {
        let src = splice_source();
        let lx = lex::lex(&src);
        let d = lx.directives.first().unwrap();
        assert_eq!((d.first, d.last), (1, 2));
        assert_eq!(lx.comments.len(), 1);
        assert_eq!((lx.comments[0].first, lx.comments[0].last), (3, 4));
        assert!(!lx.tokens.iter().any(|t| t.text == "g_hidden"));
        let s = lx.tokens.iter().find(|t| t.kind == Kind::Str).unwrap();
        assert_eq!((s.text.as_str(), s.line, s.end_line), ("\"abcd\"", 5, 6));
        let long = lx.tokens.iter().find(|t| t.text == "g_long_name").unwrap();
        assert_eq!((long.line, long.end_line), (7, 8));
        let out = strip_clean(&src, Layout::Default);
        let text = msvc6_base::srcio::decode(&out);
        assert!(text.contains("int g_x = 1;\nchar *g_s"), "{text}");
        let kept = strip_clean(&src, Layout::LineKeep);
        assert_eq!(
            lex::physical_lines(&kept).len(),
            lex::physical_lines(&src).len()
        );
    }

    #[test]
    fn line_positions_are_numbered_as_vc6_numbers_them_and_the_strip_holds_them() {
        let src = line_source();
        let lx = lex::lex(&src);
        assert!(lx.problems.is_empty(), "{:?}", lx.problems);
        let dead = lx.tokens.iter().find(|t| t.text == "prose").unwrap();
        assert_eq!(dead.cond, Cond::Dead);
        let m = MacroSet::builtin();
        let uses = linepos::uses(&lx, &m);
        assert_eq!(uses.len(), 3, "{uses:?}");
        let pins = linepos::pins(&lx);
        assert_eq!(pins.len(), 1);
        let assert_use = &uses[0];
        assert_eq!((assert_use.first, assert_use.own_last), (8, 9));
        assert_eq!(assert_use.brace, Some(7));
        assert_eq!(linepos::compiler_line(&pins, uses[2].own_last), Some(51));
        let anchored = strip_clean(&src, Layout::Anchored);
        let after = lex::lex(&anchored);
        assert!(linepos::proof(&lx, &after, &m).is_empty());
        let p = Plan::new(Syntax::C, &src, &[], &m);
        let flat = p.apply(&src, Layout::Default);
        assert!(p.check(&src, &flat, Layout::Default, &m).is_err());
        let best = p.best(&src, &m, None, None);
        assert_eq!(best.layout, Some(Layout::Anchored));
    }

    #[test]
    fn a_rename_in_a_trigraph_and_cp1252_source_is_proven() {
        let old = b"int f(int cnt)\n{\n    char *s = \"\xa7??/\"\";\n    return cnt + s[0];\n}\n";
        let new =
            b"int f(int count)\n{\n    char *s = \"\xa7??/\"\";\n    return count + s[0];\n}\n";
        let v = renamecheck::check_file("t.cpp", old, new, &renamecheck::Options::default());
        assert!(v.proven(), "{v:#?}");
        let lx = lex::lex(new);
        assert_eq!(defs::definitions(&lx).len(), 1);
    }

    fn refused(src: &[u8]) -> bool {
        let m = MacroSet::builtin();
        let p = Plan::new(Syntax::C, src, &[], &m);
        p.best(src, &m, None, None).layout.is_none()
    }

    #[test]
    fn an_asm_block_opened_in_one_conditional_branch_is_refused_not_stripped() {
        let src = b"int g;\nvoid h(void)\n{\n#ifdef USE_ASM\n    __asm {\n        mov eax, 1\n#else\n    {\n        g = 1;\n        g = 2;\n#endif\n    }\n}\n";
        let lx = lex::lex(src);
        assert!(lx.comments.is_empty(), "{:?}", lx.comments);
        assert!(!lx.problems.is_empty());
        assert!(refused(src));
        let other = b"int g;\nvoid h(void)\n{\n#ifdef USE_ASM\n    __asm\n#else\n    if (g)\n#endif\n    {\n        g = 1;\n    }\n}\n";
        assert!(refused(other));
        let agree = b"void h(void)\n{\n#ifdef USE_ASM\n    __asm { mov eax, 1 ; c\n    }\n#else\n    g = 1;\n#endif\n    g = 2; // k\n}\n";
        let lx = lex::lex(agree);
        assert!(lx.problems.is_empty(), "{:?}", lx.problems);
        assert_eq!(lx.comments.len(), 2);
    }

    #[test]
    fn dead_text_never_changes_the_asm_state() {
        let p37 = b"void f(int *p)\n{\n#if 0\n    __asm {\n        mov eax, p\n#endif\n    *p = 1;\n    p[1] = 2;\n}\n";
        let lx = lex::lex(p37);
        assert!(lx.comments.is_empty(), "{:?}", lx.comments);
        assert!(lx.problems.is_empty(), "{:?}", lx.problems);
        assert!(lx.tokens.iter().filter(|t| t.text == ";").all(|t| !t.asm));
        let p10e = b"#if 0\n    __asm {\n#endif\nint x; // c\nint y;\n";
        let lx = lex::lex(p10e);
        assert_eq!(lx.comments.len(), 1);
        assert_eq!(
            strip_clean(p10e, Layout::Default),
            b"#if 0\n    __asm {\n#endif\nint x;\nint y;\n".to_vec()
        );
        let inside = b"void f(void)\n{\n    __asm {\n        mov eax, 1\n#if 0\n    }\n#endif\n        mov ebx, 2 ; c\n    }\n    g(); // d\n}\n";
        let lx = lex::lex(inside);
        assert!(lx.problems.is_empty(), "{:?}", lx.problems);
        assert_eq!(lx.comments.len(), 2);
        assert_eq!(lx.comments[0].kind, lex::CommentKind::Asm);
    }

    #[test]
    fn a_trigraph_splice_between_star_and_slash_does_not_close_a_block_comment() {
        let p03c = b"int a; /* c *??/\n/ int b;\nint c; /* d */\nint e;\n";
        let lx = lex::lex(p03c);
        let names: Vec<&str> = lx
            .tokens
            .iter()
            .filter(|t| t.kind == Kind::Ident)
            .map(|t| t.text.as_str())
            .collect();
        assert_eq!(names, ["int", "a", "int", "e"]);
        assert_eq!(lx.comments.len(), 1);
        assert_eq!((lx.comments[0].first, lx.comments[0].last), (1, 3));
        assert_eq!(
            strip_clean(p03c, Layout::Default),
            b"int a;\nint e;\n".to_vec()
        );
        let p03b = b"int a; /* c *??/\n/ int b;\nint c;\n";
        assert!(!lex::lex(p03b).problems.is_empty());
        assert!(refused(p03b));
        let p03g = b"int a; /* x *??/\n??/\n/ int b;\nint c; /* d */\n";
        let lx = lex::lex(p03g);
        assert_eq!(lx.comments.len(), 1);
        assert!(!lx.tokens.iter().any(|t| t.text == "b" || t.text == "c"));
        let plain = b"int a; /* c *\\\n/ int b;\n";
        let lx = lex::lex(plain);
        assert!(lx.tokens.iter().any(|t| t.text == "b"));
        let opener = b"int a; /??/\n* c */ int b;\nint e;\n";
        let lx = lex::lex(opener);
        assert_eq!(lx.comments.len(), 1);
        assert!(lx.tokens.iter().any(|t| t.text == "b"));
    }

    #[test]
    fn nul_bytes_are_deleted_before_anything_reads_the_source() {
        let p35 = b"int f(void) { return 1 /\0/ 2\n; }\n";
        let lx = lex::lex(p35);
        assert_eq!(lx.comments.len(), 1);
        assert!(!lx.tokens.iter().any(|t| t.text == "2"));
        assert_eq!(
            strip_clean(p35, Layout::Default),
            b"int f(void) { return 1\n; }\n".to_vec()
        );
        let p11 = b"char s[] = \"a\0b\"; /* c */\nint x; // d \0 e\nint y;\n";
        let lx = lex::lex(p11);
        let s = lx.tokens.iter().find(|t| t.kind == Kind::Str).unwrap();
        assert_eq!(s.text, "\"ab\"");
        assert_eq!(lx.comments.len(), 2);
        assert_eq!(
            strip_clean(p11, Layout::Default),
            b"char s[] = \"a\0b\";\nint x;\nint y;\n".to_vec()
        );
        let tri = b"char c = '?\0?/'';\n";
        let lx = lex::lex(tri);
        assert_eq!(
            lx.tokens.iter().find(|t| t.kind == Kind::Chr).unwrap().text,
            "'\\''"
        );
    }

    #[test]
    fn an_error_directive_reads_its_text_raw() {
        let p38 = b"#error stop // tail\nint a; /* c */\n#error a /* b */ c\n";
        let lx = lex::lex(p38);
        assert_eq!(lx.comments.len(), 1, "{:?}", lx.comments);
        assert_eq!(lx.comments[0].first, 2);
        let d = &lx.directives[0];
        assert_eq!(lx.args(d).len(), 1);
        assert_eq!(lx.args(d)[0].text, "stop // tail");
        assert_eq!(lx.args(d)[0].kind, Kind::Text);
        let out = strip_clean(p38, Layout::Default);
        assert_eq!(
            out,
            b"#error stop // tail\nint a;\n#error a /* b */ c\n".to_vec()
        );
        let warn = b"#warning keep // this\n";
        assert!(lex::lex(warn).comments.is_empty());
        let dead = b"#if 0\n#error don't /* this\n#endif\nint after;\n";
        let lx = lex::lex(dead);
        assert!(lx.comments.is_empty());
        assert!(
            lx.tokens
                .iter()
                .any(|t| t.text == "after" && t.cond == Cond::Live)
        );
    }

    #[test]
    fn a_macro_that_expands_to_asm_moves_the_asm_state_where_it_is_used() {
        let n02a = b"#define ASM_BEGIN __asm {\nint g;\nvoid h(void)\n{\n    ASM_BEGIN\n        mov eax, 1 ; see /* here\n        mov g, eax\n        ; */\n    }\n}\n";
        let lx = lex::lex(n02a);
        assert!(lx.problems.is_empty(), "{:?}", lx.problems);
        assert_eq!(lx.comments.len(), 2);
        assert!(lx.comments.iter().all(|c| c.kind == lex::CommentKind::Asm));
        let out = strip_clean(n02a, Layout::Default);
        assert!(out.windows(10).any(|w| w == b"mov g, eax"));
        let call = b"#define SETG(v) __asm mov eax, v __asm mov g, eax\nint g;\nvoid h(void)\n{\n    SETG((7)) ; tail /* x\n    g = 1; /* y */\n}\n";
        let lx = lex::lex(call);
        assert!(lx.problems.is_empty(), "{:?}", lx.problems);
        assert_eq!(lx.comments.len(), 2, "{:?}", lx.comments);
        let out = strip_clean(call, Layout::Default);
        assert!(out.windows(6).any(|w| w == b"g = 1;"));
        let nested = b"#define A _asm\n#define B A {\nvoid h(void)\n{\n    B\n        mov eax, 1 ; /* x\n    }\n}\n";
        let lx = lex::lex(nested);
        assert_eq!(lx.comments.len(), 1);
        assert_eq!(lx.comments[0].kind, lex::CommentKind::Asm);
        let brace = b"#define END_ASM }\nint g;\nvoid h(void)\n{\n    __asm {\n        mov eax, 1\n    END_ASM\n    g = 1; /* c */ g = 2;\n}\n";
        let out = strip_clean(brace, Layout::Default);
        assert!(out.windows(13).any(|w| w == b"g = 1; g = 2;"));
        let cycle = b"#define X Y __asm\n#define Y X\nvoid h(void)\n{\n    Y {\n        mov eax, 1 ; c /* d\n    }\n}\n";
        let lx = lex::lex(cycle);
        assert!(lx.problems.is_empty(), "{:?}", lx.problems);
        assert_eq!(lx.comments.len(), 1);
        let undefined = b"#define ASM_BEGIN __asm {\n#undef ASM_BEGIN\nint ASM_BEGIN; /* c */\n";
        let lx = lex::lex(undefined);
        assert!(lx.problems.is_empty());
        assert_eq!(lx.comments[0].kind, lex::CommentKind::Block);
        let not_called = b"#define F(x) __asm {\nint F; /* c */ int k;\n";
        assert_eq!(
            lex::lex(not_called).comments[0].kind,
            lex::CommentKind::Block
        );
        let guarded = b"#ifndef H\n#define H\n#define AB __asm {\nvoid f(void)\n{\n    AB mov eax, 1 ; c /* d\n    }\n}\n#endif\n";
        let lx = lex::lex(guarded);
        assert!(lx.problems.is_empty(), "{:?}", lx.problems);
        assert_eq!(lx.comments[0].kind, lex::CommentKind::Asm);
        let apart = b"#ifdef A\n#define M __asm {\n#else\nint M; /* c */ int k;\n#endif\n";
        let lx = lex::lex(apart);
        assert!(lx.problems.is_empty(), "{:?}", lx.problems);
        assert_eq!(lx.comments[0].kind, lex::CommentKind::Block);
        let undecided = b"#ifdef A\n#define M __asm {\n#endif\nint M; /* c */ int k;\n";
        assert!(refused(undecided));
        let mut huge = b"#define A0 x x\n".to_vec();
        for n in 1..24 {
            huge.extend(format!("#define A{n} A{m} A{m}\n", m = n - 1).as_bytes());
        }
        huge.extend(b"int f(void) { A23; }\n");
        assert!(refused(&huge));
    }

    #[test]
    fn an_error_directive_in_a_group_the_macros_decide_is_read_both_ways() {
        let g01 = b"#ifdef NEVER\n#error see /* this\n#endif\nint a; /* c */\n#endif\nint b;\n#ifdef NEVER2\nint k = '*/';\n#endif\nint z;\n";
        assert!(!lex::lex(g01).problems.is_empty());
        assert!(refused(g01));
        let n06e =
            b"#ifdef NEVER\n#error see /* this\n#endif\nint a; /* c */ int b;\n#endif\nint z;\n";
        assert!(refused(n06e));
        let n06d =
            b"#ifndef Q\n#error need Q \\\n /* see */ docs // here\n#endif\nint a; /* c */\n";
        let lx = lex::lex(n06d);
        assert!(lx.problems.is_empty(), "{:?}", lx.problems);
        assert_eq!(lx.comments.len(), 1);
        let d = &lx.directives[1];
        assert_eq!(lx.args(d)[0].kind, Kind::Text);
        assert_eq!(lx.args(d)[0].text, "need Q  /* see */ docs // here");
        let quoted = b"#ifdef NEVER\n#error don't /* this\n#endif\nint z;\n";
        assert!(lex::lex(quoted).problems.is_empty());
    }

    #[test]
    fn a_conditional_directive_with_no_open_group_is_a_problem() {
        for src in [
            &b"int a;\n#endif\nint b;\n"[..],
            &b"#else\nint b;\n"[..],
            &b"#elif 1\nint b;\n"[..],
        ] {
            assert!(!lex::lex(src).problems.is_empty());
            assert!(refused(src));
        }
    }

    #[test]
    fn every_macro_and_error_probe_strips_to_its_compiler_checked_bytes_or_is_refused() {
        let dir =
            std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../fixtures/asm-macros");
        let expect = msvc6_base::srcio::own_read(dir.join("expect.tsv")).unwrap();
        let m = MacroSet::builtin();
        let mut seen = 0usize;
        for line in expect.lines() {
            let (name, want) = line.split_once('\t').unwrap();
            let src = std::fs::read(dir.join(name)).unwrap();
            let p = Plan::new(Syntax::C, &src, &[], &m);
            let got = match p.best(&src, &m, None, None).layout {
                None => "refused".to_string(),
                Some(l) => msvc6_base::digest::sha256_hex(&p.apply(&src, l)),
            };
            assert_eq!(got, want, "{name}");
            seen += 1;
        }
        let probes = std::fs::read_dir(&dir)
            .unwrap()
            .filter(|e| {
                e.as_ref()
                    .unwrap()
                    .path()
                    .extension()
                    .is_some_and(|x| x == "c")
            })
            .count();
        assert_eq!(seen, probes);
    }
}
