#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OpInfo {
    pub name: &'static str,
    pub arity: Option<(u8, u8)>,
    pub c2: &'static str,
}

const fn op(name: &'static str, pops: u8, pushes: u8, c2: &'static str) -> OpInfo {
    OpInfo {
        name,
        arity: Some((pops, pushes)),
        c2,
    }
}

const fn special(name: &'static str, c2: &'static str) -> OpInfo {
    OpInfo {
        name,
        arity: None,
        c2,
    }
}

pub const SYM: u8 = 0x26;
pub const LABEL: u8 = 0x29;
pub const PARAM: u8 = 0x2d;
pub const CONST: u8 = 0x33;
pub const BITFIELD: u8 = 0x37;
pub const BRFALSE: u8 = 0x38;
pub const BRTRUE: u8 = 0x39;
pub const GOTO: u8 = 0x3a;
pub const SWITCH: u8 = 0x3b;
pub const CASELIST: u8 = 0x3c;
pub const CASE: u8 = 0x3d;
pub const CALL: u8 = 0x3e;
pub const ICALL: u8 = 0x40;
pub const ENTRY: u8 = 0x46;
pub const EXIT: u8 = 0x47;
pub const DISCARD: u8 = 0x4b;
pub const ENDLIST: u8 = 0x4c;
pub const EOF: u8 = 0x4d;
pub const PRAGMA: u8 = 0x4f;
pub const SCOPE: u8 = 0x53;
pub const ENDSCOPE: u8 = 0x54;
pub const EXHEAD: u8 = 0x5b;
pub const THISARG: u8 = 0x99;
pub const THISARG2: u8 = 0x9a;
pub const CTEMP: u8 = 0x9b;

pub const SUB_LINE: i16 = 0x01;
pub const SUB_FILE: i16 = 0x02;
pub const SUB_OPTFLAGS: i16 = 0x1f;

#[must_use]
pub fn info(o: u8) -> OpInfo {
    match o {
        0x01 => op("nop01", 0, 0, "nothing"),
        0x02 => op("add", 2, 1, "0x16d"),
        0x03 => op("sub", 2, 1, "0x16e"),
        0x04 => op("mul", 2, 1, "0x16f"),
        0x05 => op("div", 2, 1, "0x175"),
        0x06 => op("mod", 2, 1, "0x176"),
        0x08 => op("neg", 1, 1, "0x161"),
        0x09 => op("shl", 2, 1, "0x171"),
        0x0a => op("shr", 2, 1, "0x177"),
        0x0b => op("and", 2, 1, "0x172"),
        0x0c => op("or", 2, 1, "0x173"),
        0x0d => op("xor", 2, 1, "0x174"),
        0x0e => op("bnot", 1, 1, "0x160"),
        0x0f => op("asg.add", 2, 1, "assignment op 0x16d"),
        0x10 => op("asg.sub", 2, 1, "assignment op 0x16e"),
        0x11 => op("asg.mul", 2, 1, "assignment op 0x16f"),
        0x12 => op("asg.div", 2, 1, "assignment op 0x175"),
        0x13 => op("asg.mod", 2, 1, "assignment op 0x176"),
        0x15 => op("asg.shl", 2, 1, "assignment op 0x171"),
        0x16 => op("asg.shr", 2, 1, "assignment op 0x177"),
        0x17 => op("asg.and", 2, 1, "assignment op 0x172"),
        0x18 => op("asg.xor", 2, 1, "assignment op 0x174"),
        0x19 => op("asg.or", 2, 1, "assignment op 0x173"),
        0x1a => op("lnot", 1, 1, "inverts the condition"),
        0x1b => op("oror", 2, 1, "|| as a value"),
        0x1c => op("andand", 2, 1, "&& as a value"),
        0x1f => op("cmp.eq", 2, 1, "0x17d cc 2"),
        0x20 => op("cmp.ne", 2, 1, "0x17d cc 1"),
        0x21 => op("cmp.le", 2, 1, "0x17d cc 4"),
        0x22 => op("cmp.lt", 2, 1, "0x17d cc 6"),
        0x23 => op("cmp.ge", 2, 1, "0x17d cc 3"),
        0x24 => op("cmp.gt", 2, 1, "0x17d cc 5"),
        SYM => op("sym", 0, 1, "an operand for the symbol"),
        0x27 => op(
            "addoff",
            2,
            1,
            "address + constant: on a symbol's address, the part record",
        ),
        0x28 => op(
            "index",
            2,
            1,
            "converts the right operand to int, then 0x16d",
        ),
        LABEL => op("label", 0, 0, "defines a label"),
        0x2a => op("rec2a", 0, 0, "nothing (operands read and dropped)"),
        0x2b => op("rec2b", 0, 0, "nothing (operands read and dropped)"),
        0x2c => op("cvt", 1, 1, "conversion to the type"),
        PARAM => special("param", "inside entry .. endlist: one formal"),
        0x30 => op("load", 1, 1, "indirection: the value at the address"),
        0x32 => op("asg", 2, 1, "assignment"),
        CONST => op("const", 0, 1, "constant operand"),
        0x34 => op("cvt2", 1, 1, "conversion (as 0x2c)"),
        0x35 => op("postinc", 2, 1, "x++ by the step on top"),
        0x36 => op("postdec", 2, 1, "x-- by the step on top"),
        BITFIELD => special(
            "bitfield",
            "pops the width and the bit offset onto the address below them",
        ),
        BRFALSE => op("brfalse", 1, 0, "0x185 to the label, taken when false"),
        BRTRUE => op(
            "brtrue",
            1,
            0,
            "0x185 to the label on the inverse condition",
        ),
        GOTO => op("goto", 0, 0, "0x186 to the label"),
        SWITCH => special(
            "switch",
            "copies the selector to a temp, reads the body as a nested tree",
        ),
        CASELIST => op(
            "caselist",
            1,
            0,
            "starts the case list; the label is the default",
        ),
        CASE => op("case", 1, 0, "a case: the popped constant, the label"),
        CALL => special("call", "pops the callee, reads the arguments up to endlist"),
        ICALL => special("icall", "a call of an intrinsic; the callee is its number"),
        0x41 => op("ret", 1, 0, "0x16c: the return value"),
        0x42 => op(
            "cond",
            3,
            1,
            "?: pops the false arm, the true arm, the condition",
        ),
        0x43 => op("colon", 0, 0, "nothing in C2"),
        0x44 => op("comma", 2, 1, "the left for effect, the value the right"),
        ENTRY => special(
            "entry",
            "the function's entry; formals follow up to endlist",
        ),
        EXIT => op("exit", 0, 0, "the function's exit"),
        DISCARD => op(
            "discard",
            1,
            0,
            "an expression statement: the value is dropped",
        ),
        ENDLIST => op(
            "endlist",
            0,
            0,
            "ends a formal or argument list (and a nested tree)",
        ),
        EOF => op("eof", 0, 0, "end of the stream"),
        PRAGMA => op("pragma", 0, 0, "per sub-op"),
        SCOPE => op("scope", 0, 0, "a scope opens"),
        ENDSCOPE => op(
            "endscope",
            0,
            0,
            "a scope closes; operand 0 ends the function",
        ),
        0x55 => op(
            "arg",
            1,
            1,
            "one call argument, converted to the argument type",
        ),
        0x56 => op("file", 0, 0, "the source file"),
        0x59 => op("op59", 1, 1, "0x162 when the statement list is not empty"),
        0x5a => op("loadv", 1, 1, "indirection, flagged"),
        EXHEAD => op("exhead", 0, 0, "ex header: the offset of the first body"),
        0x5c => op(
            "eh.live",
            2,
            1,
            "0x1a0: the cleanup just written becomes live",
        ),
        0x5d => op("eh.unwind", 0, 1, "pushes 0x1a1: long state, long count"),
        0x5e => op(
            "eh.leave",
            0,
            1,
            "pushes 0x1a2: the objects of a scope are destroyed",
        ),
        0x60 => op("eh.try", 0, 1, "pushes 0x1a4: a try block opens"),
        0x61 => op("eh.catch", 0, 1, "pushes 0x1a5: a handler"),
        0x62 => op("eh.trymap", 0, 1, "pushes 0x1a6"),
        0x68 => op("ptradd", 2, 1, "0x16d, result typed pointer"),
        0x77 => op(
            "nulladj",
            2,
            1,
            "adds a nonzero offset unless the pointer is null",
        ),
        0x8b => op("ptrsub", 2, 1, "0x16e on pointers"),
        0x8d => op(
            "seh.except",
            1,
            1,
            "__except: 0x198, then 0x199 on the filter value",
        ),
        0x8e => op("seh.finally", 0, 1, "__finally: pushes 0x194"),
        0x8f => op("seh.try", 0, 1, "__try: pushes 0x192"),
        0x90 => op(
            "seh.end",
            0,
            1,
            "the end of a __try statement: pushes 0x193",
        ),
        THISARG => op("thisarg", 0, 0, "the next call takes an object"),
        THISARG2 => op("thisarg2", 0, 0, "the next call takes an object, two words"),
        CTEMP => op("ctemp", 0, 1, "a front-end temporary"),
        0x9d => special("op9d", "read by C2, not modelled"),
        0xa0 => op("assume", 1, 1, "__assume: the stack entry stays"),
        _ => special("?", "not an op C2's reader handles"),
    }
}

#[must_use]
pub fn pragma_name(sub: i16) -> &'static str {
    match sub {
        0x01 => "line",
        0x02 => "file",
        0x0f => "mark1b4",
        0x10 => "mark1b5",
        0x11 => "dbg1b6",
        0x12 => "dbg1b7",
        0x16 => "inasm",
        0x18 => "mark1ba",
        0x19 => "op19",
        0x1a => "op1a",
        0x1f => "optflags",
        0x20 => "mark1b8",
        0x23 => "warnings",
        0x24 => "line24",
        0x25 => "op25",
        _ => "?",
    }
}
