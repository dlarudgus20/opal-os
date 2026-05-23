#![no_std]

extern crate self as opal_kernel;

pub mod console;
pub mod platform;

#[cfg(opal_kernel_test)]
pub mod ktest;

use core::panic::PanicInfo;

use opal_ktest::ktest;

#[unsafe(no_mangle)]
pub extern "C" fn kmain() -> ! {
    console::init();

    #[cfg(opal_kernel_test)]
    {
        ktest::run();
    }

    #[cfg(not(opal_kernel_test))]
    {
        console::write_str("opal rust kernel: smoke boot\n");
        platform::halt_loop();
    }
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    console::write_str("opal rust kernel: panic\n");

    #[cfg(opal_kernel_test)]
    ktest::exit_failure();

    #[cfg(not(opal_kernel_test))]
    platform::halt_loop();
}

#[ktest]
fn smoke_test() {
    assert_eq!(1, 1);
}
