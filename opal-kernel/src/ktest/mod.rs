use crate::console::print;

#[repr(C)]
pub struct KernelTest {
    pub name_ptr: *const u8,
    pub name_len: usize,
    pub func: extern "C" fn(),
}

unsafe impl Sync for KernelTest {}

impl KernelTest {
    fn name(&self) -> &'static str {
        let bytes = unsafe { core::slice::from_raw_parts(self.name_ptr, self.name_len) };
        unsafe { core::str::from_utf8_unchecked(bytes) }
    }
}

unsafe extern "C" {
    static __ktest_start: KernelTest;
    static __ktest_end: KernelTest;
}

pub fn run() -> ! {
    let mut run_count = 0usize;

    print!("\n==== unit test run ====\n");

    let mut current = &raw const __ktest_start;
    let end = &raw const __ktest_end;
    while current < end {
        let test = unsafe { &*current };
        print!("[ RUN      ] {}\n", test.name());

        (test.func)();

        print!("[       OK ] {}\n", test.name());
        run_count += 1;

        current = unsafe { current.add(1) };
    }

    print!("==== unit test end ==== run={} pass={} fail=0\n", run_count, run_count);
    exit(true);
}

fn exit(_success: bool) -> ! {
    #[cfg(opal_ktest)]
    {
        use crate::platform::qemu;
        qemu::debug_exit(if _success { qemu::EXIT_SUCCESS } else { qemu::EXIT_FAILURE });
    }

    #[cfg(not(opal_ktest))]
    {
        use crate::arch::halt_loop;
        halt_loop();
    }
}

pub fn exit_failure() -> ! {
    exit(false);
}
