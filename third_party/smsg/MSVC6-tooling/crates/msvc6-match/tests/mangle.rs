#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_match::mangle::{Kind, SigLevel, compare, cw, itanium, kind_of, norm_type, parse_text};

#[test]
fn codewarrior_names_read_as_scope_name_and_parameters() {
    let s = cw(".send__6SocketFPviUli").unwrap();
    assert_eq!(s.key(), "Socket::send(void*,int,unsigned long,int)");
    let s = cw("__ct__6WidgetFv").unwrap();
    assert_eq!(s.key(), "Widget::Widget()");
    assert_eq!(s.has_this(), Some(true));
    let s = cw("close_all__6WidgetSFv").unwrap();
    assert_eq!(s.has_this(), Some(false));
    let s = cw("place__6WidgetFiiiiPciP6WidgetP4MenuP6Border").unwrap();
    assert_eq!(
        s.key(),
        "Widget::place(int,int,int,int,char*,int,Widget*,Menu*,Border*)"
    );
    assert!(cw("height__6WidgetCFv").unwrap().konst);
    let s = cw("mix__Fii").unwrap();
    assert_eq!(s.key(), "mix(int,int)");
    assert_eq!(s.has_this(), Some(false));
    let s = cw("on_tick__6WidgetFiPFv_iP6Widget").unwrap();
    assert_eq!(s.key(), "Widget::on_tick(int,fnptr,Widget*)");
    let s = cw("__dt__20List<12StringRecord>Fv").unwrap();
    assert_eq!(s.class(), Some("List<StringRecord>"));
    assert_eq!(s.name, "{dtor}");
    assert_eq!(cw("pick__3BagFiT1").unwrap().params, ["int", "int"]);
    assert_eq!(
        cw("pick__3BagFPcN21").unwrap().params,
        ["char*", "char*", "char*"]
    );
    assert!(!cw(".OSAllocate").unwrap().params_known);
    let s = cw("default_font__6Button").unwrap();
    assert_eq!(s.qual_name(), "Button::default_font");
    assert!(!s.params_known);
    assert_eq!(cw("__dl__4PoolFPv").unwrap().has_this(), Some(false));
    assert_eq!(
        cw("blit__6BufferFPcP7tagRECTi").unwrap().params,
        ["char*", "tagRECT*", "int"]
    );
    let s = cw("f__Q23Foo3BarFRC3Baz").unwrap();
    assert_eq!(s.scope, ["Foo", "Bar"]);
    assert_eq!(s.params, ["Baz&"]);
    assert_eq!(
        cw("remap__6BufferFiiiiPUc")
            .unwrap()
            .params
            .last()
            .map(String::as_str),
        Some("unsigned char*")
    );
    assert!(cw("__nothing").is_none());
    assert!(cw("").is_none());
}

#[test]
fn itanium_names_read_through_their_demangled_text() {
    let s = itanium("_ZN6Widget5placeEiiiiPciPS_P4MenuP6Border").unwrap();
    assert_eq!(
        s.key(),
        "Widget::place(int,int,int,int,char*,int,Widget*,Menu*,Border*)"
    );
    assert_eq!(itanium("_ZN6WidgetC1Ev").unwrap().name, "{ctor}");
    assert_eq!(itanium("_ZN6WidgetD0Ev").unwrap().name, "{dtor}");
    assert!(itanium("_ZNK6Widget6heightEv").unwrap().konst);
    let s = itanium("_ZN4PooldlEPv").unwrap();
    assert_eq!(s.name, "operator delete");
    assert_eq!(s.has_this(), Some(false));
    assert_eq!(
        itanium("_ZN6Widget7on_tickEiPFivEPS_").unwrap().params,
        ["int", "fnptr", "Widget*"]
    );
    assert_eq!(
        itanium("_ZN4ListI12StringRecordE3addEPS0_")
            .unwrap()
            .class(),
        Some("List<StringRecord>")
    );
    assert_eq!(
        itanium("_ZN6BufferaSERKS_").unwrap().key(),
        "Buffer::operator=(Buffer&)"
    );
    let s = itanium("_ZN6Widget5stackE").unwrap();
    assert_eq!(s.qual_name(), "Widget::stack");
    assert!(!s.params_known);
    assert_eq!(itanium("_Z3mixii").unwrap().key(), "mix(int,int)");
    assert_eq!(itanium("memcpy").unwrap().name, "memcpy");
    assert!(itanium("_Zgarbage").is_none());
}

#[test]
fn definition_lines_read_as_signatures() {
    assert_eq!(
        parse_text("int Surface::Clear(int color)", None)
            .unwrap()
            .key(),
        "Surface::Clear(int)"
    );
    assert_eq!(
        parse_text("~Holder() { Clear(); }", Some("Holder<struct Item>"))
            .unwrap()
            .name,
        "{dtor}"
    );
    assert_eq!(parse_text("Panel::Panel()", None).unwrap().name, "{ctor}");
    assert_eq!(
        parse_text("virtual void Release(T *elem) {}", Some("Holder<Item>"))
            .unwrap()
            .params,
        ["T*"]
    );
    assert_eq!(
        parse_text("int ReadSize(const char *file, int *w, int *h)", None)
            .unwrap()
            .key(),
        "ReadSize(char*,int*,int*)"
    );
    assert_eq!(
        parse_text(
            "unsigned char Surface::Red(int x, int y, unsigned char d)",
            None
        )
        .unwrap()
        .params,
        ["int", "int", "unsigned char"]
    );
    assert_eq!(
        parse_text("inline Renderer *Surface::Owner()", None)
            .unwrap()
            .key(),
        "Surface::Owner()"
    );
    assert_eq!(
        parse_text("void Widget::on_tick(int a, int (*pfn)(), Widget *w)", None)
            .unwrap()
            .params,
        ["int", "fnptr", "Widget*"]
    );
    let s = parse_text("int Foo::Bar(void) const", None).unwrap();
    assert!(s.konst && s.params.is_empty());
    let s = parse_text("void *Pool::operator new(unsigned int size)", None).unwrap();
    assert_eq!(s.name, "operator new");
    assert_eq!(s.has_this(), Some(false));
    assert_eq!(
        parse_text("Entry::Entry() : Panel()", None).unwrap().name,
        "{ctor}"
    );
    let s = parse_text("void Log(char *fmt, ...)", None).unwrap();
    assert!(s.varargs && s.params == ["char*"]);
    assert_eq!(
        parse_text("int f(char buf[256], unsigned int n)", None)
            .unwrap()
            .params,
        ["char*", "unsigned"]
    );
    assert_eq!(
        parse_text("void Log(const char *tag, int value1, int a[10])", None)
            .unwrap()
            .params,
        ["char*", "int", "int*"]
    );
    let s = parse_text("Widget::stack", None).unwrap();
    assert!(!s.params_known);
    assert_eq!(s.qual_name(), "Widget::stack");
}

#[test]
fn signature_levels_order_mismatch_count_kinds_exact() {
    let theirs = cw("fill__6BufferFi").unwrap();
    let ours = parse_text("void Surface::Fill(unsigned color)", None).unwrap();
    assert_eq!(compare(&theirs, &ours, Some(true)), SigLevel::Kinds);
    let two = parse_text("void Surface::Fill(int a, int b)", None).unwrap();
    assert_eq!(compare(&theirs, &two, Some(true)), SigLevel::Mismatch);
    let ptr = parse_text("void Surface::Fill(char *p)", None).unwrap();
    assert_eq!(compare(&theirs, &ptr, Some(true)), SigLevel::Count);
    let free = parse_text("void Fill(int a)", None).unwrap();
    assert_eq!(compare(&theirs, &free, Some(false)), SigLevel::Mismatch);
    assert_eq!(
        compare(&cw(".OSAllocate").unwrap(), &free, None),
        SigLevel::Unknown
    );
    let log = cw("log__FPcPciii").unwrap();
    let mine = parse_text(
        "void Log(const char *t, const char *d, int a, int b, int c)",
        None,
    )
    .unwrap();
    assert_eq!(compare(&log, &mine, None), SigLevel::Exact);
}

#[test]
fn types_normalise_and_classify() {
    assert_eq!(norm_type("const struct Foo *"), "Foo*");
    assert_eq!(norm_type("unsigned int"), "unsigned");
    assert_eq!(norm_type("long int"), "long");
    assert_eq!(norm_type("signed"), "int");
    assert_eq!(norm_type("char []"), "char*");
    assert_eq!(norm_type("int (*)(int)"), "fnptr");
    assert_eq!(kind_of("DWORD"), Kind::Int);
    assert_eq!(kind_of("HWND"), Kind::Ptr);
    assert_eq!(kind_of("LPRECT"), Kind::Ptr);
    assert_eq!(kind_of("double"), Kind::Float);
    assert_eq!(kind_of("Thing"), Kind::Unknown);
}

#[test]
fn hostile_names_stop_at_their_nesting_bound() {
    let deep = format!("f__FP{}i", "Fv_".repeat(5000));
    let _ = msvc6_fixture::backstop::run("nested function types", || cw(&deep));
    let templ = format!("f__{}Fv", "Q9".repeat(4000));
    let _ = msvc6_fixture::backstop::run("qualified names", || cw(&templ));
    let long = format!("int f({})", "int,".repeat(20_000));
    let s = msvc6_fixture::backstop::run("a long parameter list", || parse_text(&long, None)).0;
    assert!(s.is_some_and(|s| s.params.len() >= 20_000));
    let it = format!("_Z1f{}", "PFv".repeat(5000));
    let _ = msvc6_fixture::backstop::run("a deep Itanium name", || itanium(&it));
}
