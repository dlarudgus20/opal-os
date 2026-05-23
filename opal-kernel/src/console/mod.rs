use crate::platform::{in8, out8};

const COM1: u16 = 0x3f8;

pub fn init() {
    unsafe {
        out8(COM1 + 1, 0x00);
        out8(COM1 + 3, 0x80);
        out8(COM1, 0x01);
        out8(COM1 + 1, 0x00);
        out8(COM1 + 3, 0x03);
        out8(COM1 + 2, 0xc7);
        out8(COM1 + 4, 0x0b);
    }
}

pub fn write_str(s: &str) {
    for byte in s.bytes() {
        write_byte(byte);
    }
}

fn write_byte(byte: u8) {
    while unsafe { in8(COM1 + 5) } & 0x20 == 0 {}
    unsafe {
        out8(COM1, byte);
    }
}

pub fn write_fmt(args: core::fmt::Arguments) {
    use core::fmt::Write;
    ConsoleWriter.write_fmt(args).unwrap();
}

pub struct ConsoleWriter;

impl core::fmt::Write for ConsoleWriter {
    fn write_str(&mut self, s: &str) -> core::fmt::Result {
        write_str(s);
        Ok(())
    }
}

#[macro_export]
macro_rules! console_print {
    ($($arg:tt)*) => {
        $crate::console::write_fmt(format_args!($($arg)*));
    };
}

pub use console_print as print;
