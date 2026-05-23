use crate::platform::*;

pub const EXIT_SUCCESS: u32 = 0x10;
pub const EXIT_FAILURE: u32 = 0x11;

const QEMU_DEBUG_EXIT_PORT: u16 = 0xf4;

pub fn debug_exit(code: u32) -> ! {
    unsafe {
        out32(QEMU_DEBUG_EXIT_PORT, code);
    }
    halt_loop();
}
