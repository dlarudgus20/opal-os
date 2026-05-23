use crate::platform;

pub fn init() {
    platform::console_init();
}

pub fn write_str(s: &str) {
    for byte in s.bytes() {
        write_byte(byte);
    }
}

fn write_byte(byte: u8) {
    platform::console_write_byte(byte);
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
