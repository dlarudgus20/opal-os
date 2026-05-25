pub mod earlyserial;

#[cfg(opal_ktest)]
pub mod qemu;

use crate::arch::{in8, out8};

const COM1: u16 = 0x3f8;

pub fn console_init() {
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

pub fn console_write_byte(byte: u8) {
    while unsafe { in8(COM1 + 5) } & 0x20 == 0 {}
    unsafe {
        out8(COM1, byte);
    }
}
