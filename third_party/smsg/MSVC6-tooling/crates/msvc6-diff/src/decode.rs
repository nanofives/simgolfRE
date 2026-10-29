use iced_x86::{
    Decoder, DecoderOptions, Instruction, InstructionInfoFactory, OpAccess, OpKind, Register,
};

pub const BITS: u32 = 32;

#[derive(Debug, Clone)]
pub struct Dec {
    pub ip: u32,
    pub insn: Instruction,
    pub key: String,
    pub esp_store: bool,
}

impl Dec {
    #[must_use]
    pub fn len(&self) -> usize {
        self.insn.len()
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.insn.len() == 0
    }

    #[must_use]
    pub fn end(&self) -> usize {
        (self.ip as usize).saturating_add(self.insn.len())
    }
}

#[must_use]
pub fn decode(bytes: &[u8], ip: u64) -> Vec<Instruction> {
    let mut d = Decoder::with_ip(BITS, bytes, ip, DecoderOptions::NONE);
    let mut out = Vec::new();
    while d.can_decode() {
        out.push(d.decode());
    }
    out
}

#[must_use]
pub fn decode_one(bytes: &[u8], ip: u64) -> Instruction {
    let mut d = Decoder::with_ip(BITS, bytes, ip, DecoderOptions::NONE);
    d.decode()
}

pub type Masked<'a> = dyn Fn(u32, u32, u64) -> bool + 'a;

#[must_use]
pub fn decode_keyed(bytes: &[u8], masked: &Masked<'_>, mask_regs: bool) -> Vec<Dec> {
    let reg = |r: Register| {
        if mask_regs && r != Register::None {
            "R".to_string()
        } else {
            format!("{r:?}")
        }
    };
    let mut out = Vec::new();
    let mut info = InstructionInfoFactory::new();
    for insn in decode(bytes, 0) {
        let ip = u32::try_from(insn.ip()).unwrap_or(u32::MAX);
        let len = u32::try_from(insn.len()).unwrap_or(0);
        let mut key = format!("{:?}", insn.code());
        for op in 0..insn.op_count() {
            match insn.op_kind(op) {
                OpKind::Register => {
                    key.push(' ');
                    key.push_str(&reg(insn.op_register(op)));
                }
                OpKind::Memory => {
                    let base = insn.memory_base();
                    let disp = u64::from(insn.memory_displacement32());
                    let shown = if base == Register::ESP || masked(ip, len, disp) {
                        "D".to_string()
                    } else {
                        format!("{disp:#x}")
                    };
                    key.push_str(&format!(
                        " [{}+{}*{}+{shown}]",
                        reg(base),
                        reg(insn.memory_index()),
                        insn.memory_index_scale()
                    ));
                }
                OpKind::NearBranch32 | OpKind::NearBranch16 | OpKind::FarBranch32 => {
                    key.push_str(" T");
                }
                _ => {
                    let v = insn.try_immediate(op).unwrap_or(0);
                    if masked(ip, len, v) {
                        key.push_str(" I");
                    } else {
                        key.push_str(&format!(" {v:#x}"));
                    }
                }
            }
        }
        let esp_store = info.info(&insn).used_memory().iter().any(|m| {
            m.base() == Register::ESP
                && matches!(
                    m.access(),
                    OpAccess::Write
                        | OpAccess::ReadWrite
                        | OpAccess::CondWrite
                        | OpAccess::ReadCondWrite
                )
        });
        out.push(Dec {
            ip,
            insn,
            key,
            esp_store,
        });
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_register_masked_key_reads_every_register_as_r() {
        let b = [0x89, 0xd8, 0x89, 0xd9];
        let plain = decode_keyed(&b, &|_, _, _| false, false);
        let masked = decode_keyed(&b, &|_, _, _| false, true);
        assert_eq!(plain.len(), 2);
        assert_ne!(plain[0].key, plain[1].key);
        assert_eq!(masked[0].key, masked[1].key);
        assert!(masked[0].key.contains(" R"));
    }

    #[test]
    fn an_esp_displacement_and_a_masked_address_key_as_d() {
        let b = [0x8b, 0x44, 0x24, 0x08, 0x8b, 0x44, 0x24, 0x0c];
        let d = decode_keyed(&b, &|_, _, _| false, false);
        assert_eq!(d[0].key, d[1].key);
        let g = [0xa1, 0x00, 0x10, 0x40, 0x00];
        let open = decode_keyed(&g, &|_, _, _| false, false);
        let shut = decode_keyed(&g, &|_, _, v| v == 0x0040_1000, false);
        assert!(open[0].key.contains("0x401000"), "{}", open[0].key);
        assert!(shut[0].key.contains("+D]"), "{}", shut[0].key);
    }
}
