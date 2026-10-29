use iced_x86::{
    Decoder, DecoderOptions, FlowControl, Instruction, InstructionInfoFactory, Mnemonic, OpAccess,
    OpKind, Register,
};
use serde::Serialize;

pub const WINDOW: usize = 64;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum This {
    Ecx,
    FirstArgument,
    Unknown,
}

fn reads(access: OpAccess) -> bool {
    matches!(
        access,
        OpAccess::Read | OpAccess::CondRead | OpAccess::ReadWrite | OpAccess::ReadCondWrite
    )
}

fn writes(access: OpAccess) -> bool {
    matches!(
        access,
        OpAccess::Write | OpAccess::CondWrite | OpAccess::ReadWrite | OpAccess::ReadCondWrite
    )
}

fn depth_change(i: &Instruction) -> Option<i64> {
    match i.mnemonic() {
        Mnemonic::Push => Some(4),
        Mnemonic::Pop => Some(-4),
        Mnemonic::Sub | Mnemonic::Add
            if i.op0_kind() == OpKind::Register
                && i.op0_register() == Register::ESP
                && matches!(i.op1_kind(), OpKind::Immediate8to32 | OpKind::Immediate32) =>
        {
            let v = i64::from(i.immediate32().cast_signed());
            Some(if i.mnemonic() == Mnemonic::Sub { v } else { -v })
        }
        _ => None,
    }
}

#[must_use]
pub fn this_of(body: &[u8]) -> This {
    let mut d = Decoder::with_ip(32, body, 0, DecoderOptions::NONE);
    let mut info = InstructionInfoFactory::new();
    let mut depth = 0i64;
    let mut ecx_written = false;
    let mut holders: Vec<Register> = Vec::new();
    for _ in 0..WINDOW {
        if !d.can_decode() {
            break;
        }
        let i = d.decode();
        if i.is_invalid() {
            return This::Unknown;
        }
        let used = info.info(&i);
        let op0 = used.op0_access();
        if !ecx_written {
            for r in used.used_registers() {
                if r.register().full_register32() == Register::ECX {
                    if reads(r.access()) {
                        return This::Ecx;
                    }
                    if writes(r.access()) {
                        ecx_written = true;
                    }
                }
            }
        }
        for m in used.used_memory() {
            if holders.contains(&m.base()) {
                return This::FirstArgument;
            }
        }
        if i.mnemonic() == Mnemonic::Mov
            && i.op0_kind() == OpKind::Register
            && i.op1_kind() == OpKind::Memory
            && i.memory_base() == Register::ESP
            && i.memory_index() == Register::None
            && i64::from(i.memory_displacement32()) == depth + 4
        {
            holders.push(i.op0_register());
        } else if i.op0_kind() == OpKind::Register {
            let r = i.op0_register();
            if writes(op0) {
                holders.retain(|h| *h != r);
            }
        }
        if let Some(c) = depth_change(&i) {
            depth += c;
        }
        if matches!(
            i.flow_control(),
            FlowControl::Return | FlowControl::UnconditionalBranch | FlowControl::IndirectBranch
        ) {
            break;
        }
    }
    This::Unknown
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_body_that_reads_ecx_first_is_thiscall() {
        assert_eq!(this_of(&[0x8b, 0x41, 0x14, 0xc3]), This::Ecx);
        assert_eq!(
            this_of(&[0x56, 0x8b, 0xf1, 0x8b, 0x06, 0x5e, 0xc3]),
            This::Ecx
        );
    }

    #[test]
    fn a_body_that_dereferences_its_first_stack_argument_is_stdcall() {
        assert_eq!(
            this_of(&[0x8b, 0x44, 0x24, 0x04, 0x8b, 0x48, 0x60, 0xc2, 0x08, 0x00]),
            This::FirstArgument
        );
        assert_eq!(
            this_of(&[
                0x56, 0x8b, 0x74, 0x24, 0x08, 0x8b, 0x06, 0x5e, 0xc2, 0x04, 0x00
            ]),
            This::FirstArgument
        );
    }

    #[test]
    fn a_body_that_uses_neither_is_unknown() {
        assert_eq!(
            this_of(&[0x8b, 0x44, 0x24, 0x04, 0x03, 0xc0, 0xc2, 0x04, 0x00]),
            This::Unknown
        );
        assert_eq!(this_of(&[0x33, 0xc0, 0xc3]), This::Unknown);
        assert_eq!(
            this_of(&[0xb9, 0, 0, 0, 0, 0x8b, 0x01, 0xc3]),
            This::Unknown
        );
        assert_eq!(this_of(&[0xff, 0xff]), This::Unknown);
    }
}
