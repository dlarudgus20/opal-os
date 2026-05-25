use crate::console::{ConsoleWriter, register_writer};
use crate::arch::{in8, out8};

const COM1: u16 = 0x3f8;

struct EarlySerial {
    writer: ConsoleWriter,
}

fn init() -> EarlySerial {
    unsafe {
        out8(COM1 + 1, 0x00);
        out8(COM1 + 3, 0x80);
        out8(COM1, 0x01);
        out8(COM1 + 1, 0x00);
        out8(COM1 + 3, 0x03);
        out8(COM1 + 2, 0xc7);
        out8(COM1 + 4, 0x0b);
    }
    EarlySerial
}

fn write_str()

impl EarlySerial {
    pub fn write_str(&mut self, s: &str) {
        for byte in s.bytes() {
            self.write_byte(byte);
        }
    }

    fn write_byte(&mut self, byte: u8) {
        while unsafe { in8(COM1 + 5) } & 0x20 == 0 {}
        unsafe {
            out8(COM1, byte);
        }
    }
}
