#![no_std]

extern crate self as opal_kernel;

pub mod arch;
pub mod console;
pub mod platform;

#[cfg(opal_ktest)]
pub mod ktest;

use core::panic::PanicInfo;

use opal_ktest::ktest;

#[unsafe(no_mangle)]
pub extern "C" fn kmain() -> ! {
    console::init();

    #[cfg(opal_ktest)]
    {
        ktest::run();
    }

    #[cfg(not(opal_ktest))]
    {
        console::write_str("opal rust kernel: smoke boot\n");
        arch::halt_loop();
    }
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    console::write_str("opal rust kernel: panic\n");

    #[cfg(opal_ktest)]
    ktest::exit_failure();

    #[cfg(not(opal_ktest))]
    arch::halt_loop();
}

#[ktest]
fn smoke_test() {
    assert_eq!(1, 1);
}
