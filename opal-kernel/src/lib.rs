#![no_std]
#![cfg_attr(test, feature(custom_test_frameworks))]
#![cfg_attr(test, test_runner(test_runner))]

use core::arch::asm;
use core::panic::PanicInfo;

const COM1: u16 = 0x3f8;

#[unsafe(no_mangle)]
pub extern "C" fn kmain() -> ! {
    serial_init();
    serial_write_str("opal rust kernel: smoke boot\n");
    halt_loop();
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    serial_write_str("opal rust kernel: panic\n");
    halt_loop();
}

pub fn test_runner(_tests: &[&dyn Fn()]) {}

fn serial_init() {
    unsafe {
        outb(COM1 + 1, 0x00);
        outb(COM1 + 3, 0x80);
        outb(COM1, 0x01);
        outb(COM1 + 1, 0x00);
        outb(COM1 + 3, 0x03);
        outb(COM1 + 2, 0xc7);
        outb(COM1 + 4, 0x0b);
    }
}

fn serial_write_str(s: &str) {
    for byte in s.bytes() {
        serial_write_byte(byte);
    }
}

fn serial_write_byte(byte: u8) {
    while unsafe { inb(COM1 + 5) } & 0x20 == 0 {}
    unsafe {
        outb(COM1, byte);
    }
}

fn halt_loop() -> ! {
    loop {
        unsafe {
            asm!("cli; hlt", options(nomem, nostack, preserves_flags));
        }
    }
}

unsafe fn outb(port: u16, value: u8) {
    unsafe {
        asm!(
            "out dx, al",
            in("dx") port,
            in("al") value,
            options(nomem, nostack, preserves_flags)
        );
    }
}

unsafe fn inb(port: u16) -> u8 {
    let value: u8;
    unsafe {
        asm!(
            "in al, dx",
            in("dx") port,
            out("al") value,
            options(nomem, nostack, preserves_flags)
        );
    }
    value
}
