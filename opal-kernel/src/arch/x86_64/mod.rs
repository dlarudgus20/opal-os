use core::arch::asm;

pub const RFLAGS_INTERRUPT: u64 = 1 << 9;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct CpuidInfo {
    pub eax: u32,
    pub ebx: u32,
    pub ecx: u32,
    pub edx: u32,
}

#[inline]
pub fn halt_loop() -> ! {
    loop {
        disable_interrupts();
        wait_for_interrupt();
    }
}

// Safety: I/O
#[inline]
pub unsafe fn out8(port: u16, value: u8) {
    unsafe {
        asm!(
            "out dx, al",
            in("dx") port,
            in("al") value,
            options(nomem, nostack, preserves_flags)
        );
    }
}

// Safety: I/O
#[inline]
pub unsafe fn out16(port: u16, value: u16) {
    unsafe {
        asm!(
            "out dx, ax",
            in("dx") port,
            in("ax") value,
            options(nomem, nostack, preserves_flags)
        );
    }
}

// Safety: I/O
#[inline]
pub unsafe fn out32(port: u16, value: u32) {
    unsafe {
        asm!(
            "out dx, eax",
            in("dx") port,
            in("eax") value,
            options(nomem, nostack, preserves_flags)
        );
    }
}

// Safety: I/O
#[inline]
pub unsafe fn in8(port: u16) -> u8 {
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

// Safety: I/O
#[inline]
pub unsafe fn in16(port: u16) -> u16 {
    let value: u16;
    unsafe {
        asm!(
            "in ax, dx",
            in("dx") port,
            out("ax") value,
            options(nomem, nostack, preserves_flags)
        );
    }
    value
}

// Safety: I/O
#[inline]
pub unsafe fn in32(port: u16) -> u32 {
    let value: u32;
    unsafe {
        asm!(
            "in eax, dx",
            in("dx") port,
            out("eax") value,
            options(nomem, nostack, preserves_flags)
        );
    }
    value
}

// Safety: I/O
#[inline]
pub unsafe fn insw(port: u16, dst: &mut [u16]) {
    unsafe {
        asm!(
            "rep insw",
            in("dx") port,
            inout("rdi") dst.as_mut_ptr() => _,
            inout("rcx") dst.len() => _,
            options(nostack, preserves_flags)
        );
    }
}

// Safety: I/O
#[inline]
pub unsafe fn outsw(port: u16, src: &[u16]) {
    unsafe {
        asm!(
            "rep outsw",
            in("dx") port,
            inout("rsi") src.as_ptr() => _,
            inout("rcx") src.len() => _,
            options(nostack, preserves_flags)
        );
    }
}

#[inline]
pub fn read_rflags() -> u64 {
    let flags: u64;
    unsafe {
        asm!("pushfq; pop {}", out(reg) flags, options(preserves_flags));
    }
    flags
}

// Safety: `flags` must be valid.
#[inline]
pub unsafe fn write_rflags(flags: u64) {
    unsafe {
        asm!("push {}; popfq", in(reg) flags);
    }
}

#[inline]
pub fn interrupts_enabled() -> bool {
    (read_rflags() & RFLAGS_INTERRUPT) != 0
}

#[inline]
pub fn disable_interrupts() {
    unsafe {
        asm!("cli", options(nostack));
    }
}

#[inline]
pub fn enable_interrupts() {
    unsafe {
        asm!("sti", options(nostack));
    }
}

#[inline]
pub fn wait_for_interrupt() {
    unsafe {
        asm!("hlt", options(nomem, nostack, preserves_flags));
    }
}

#[inline]
pub fn enable_interrupts_and_wait() {
    unsafe {
        asm!("sti; hlt", options(nostack));
    }
}

#[inline]
pub fn read_cr0() -> u64 {
    let value: u64;
    unsafe {
        asm!("mov {}, cr0", out(reg) value, options(nomem, nostack, preserves_flags));
    }
    value
}

#[inline]
pub fn read_cr2() -> u64 {
    let value: u64;
    unsafe {
        asm!("mov {}, cr2", out(reg) value, options(nomem, nostack, preserves_flags));
    }
    value
}

#[inline]
pub fn read_cr3() -> u64 {
    let value: u64;
    unsafe {
        asm!("mov {}, cr3", out(reg) value, options(nomem, nostack, preserves_flags));
    }
    value
}

#[inline]
pub fn read_cr4() -> u64 {
    let value: u64;
    unsafe {
        asm!("mov {}, cr4", out(reg) value, options(nomem, nostack, preserves_flags));
    }
    value
}

// Safety: `value` and its side effects must be valid.
#[inline]
pub unsafe fn write_cr0(value: u64) {
    unsafe {
        asm!("mov cr0, {}", in(reg) value, options(nostack));
    }
}

// Safety: `value` and its side effects must be valid.
#[inline]
pub unsafe fn write_cr3(value: u64) {
    unsafe {
        asm!("mov cr3, {}", in(reg) value, options(nostack));
    }
}

// Safety: `value` and its side effects must be valid.
#[inline]
pub unsafe fn write_cr4(value: u64) {
    unsafe {
        asm!("mov cr4, {}", in(reg) value, options(nostack));
    }
}

// Safety: `addr` must be a canonical virtual address whose TLB entry may be invalidated.
#[inline]
pub unsafe fn flush_tlb_addr(addr: usize) {
    unsafe {
        asm!("invlpg [{}]", in(reg) addr, options(nostack));
    }
}

#[inline]
pub fn cpuid(leaf: u32, subleaf: u32) -> CpuidInfo {
    let eax: u32;
    let ebx: u32;
    let ecx: u32;
    let edx: u32;

    unsafe {
        asm!(
            "push rbx",
            "cpuid",
            "mov {ebx_out:e}, ebx",
            "pop rbx",
            inlateout("eax") leaf => eax,
            inlateout("ecx") subleaf => ecx,
            lateout("edx") edx,
            ebx_out = lateout(reg) ebx,
            options(preserves_flags)
        );
    }

    CpuidInfo { eax, ebx, ecx, edx }
}

// Safety: `msr` must be a valid MSR index.
#[inline]
pub unsafe fn read_msr(msr: u32) -> u64 {
    let low: u32;
    let high: u32;
    unsafe {
        asm!(
            "rdmsr",
            in("ecx") msr,
            out("eax") low,
            out("edx") high,
            options(nomem, nostack, preserves_flags)
        );
    }
    ((high as u64) << 32) | (low as u64)
}

// Safety: `msr` must be a valid MSR index and `value` must be a valid value for that MSR.
#[inline]
pub unsafe fn write_msr(msr: u32, value: u64) {
    let low = value as u32;
    let high = (value >> 32) as u32;
    unsafe {
        asm!(
            "wrmsr",
            in("ecx") msr,
            in("eax") low,
            in("edx") high,
            options(nomem, nostack, preserves_flags)
        );
    }
}
