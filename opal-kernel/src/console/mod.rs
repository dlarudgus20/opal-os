use core::fmt::{self, Write};

use crate::utils::singlylist::{List, Node};
use crate::sync::{UnsafeOnce, irqspin::Mutex};

pub struct ConsoleWriter {
    node: Node,
    writer: fn (&str),
}

struct Console {
    list: List,
}

static CONSOLE: UnsafeOnce<Mutex<Console>> = UnsafeOnce::uninit();

pub fn init() {
    unsafe {
        CONSOLE.init(Mutex::new(Console { list: List::new() }));
    }
}

pub fn register_writer(writer: &mut ConsoleWriter) {
    let cons = unsafe { CONSOLE.get() }.lock();
    cons.list.push_front(&mut writer.node);
}

pub fn write_str(s: &str) {
    let cons = unsafe { CONSOLE.get() }.lock();
    for writer in cons.list.iter() {
        writer.writer(s);
    }
}

pub fn write_fmt(args: fmt::Arguments) {
    let cons = unsafe { CONSOLE.get() }.lock();
    cons.write_fmt(args).unwrap();
}

impl Write for Console {
    fn write_str(&mut self, s: &str) -> fmt::Result {
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
