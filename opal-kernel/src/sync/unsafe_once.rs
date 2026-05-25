use core::cell::UnsafeCell;
use core::mem::MaybeUninit;

pub struct UnsafeOnce<T> {
    value: UnsafeCell<MaybeUninit<T>>,
}

impl<T> UnsafeOnce<T> {
    pub const fn uninit() -> Self {
        Self { value: UnsafeCell::new(MaybeUninit::uninit()) }
    }

    /// # Safety
    /// The caller must ensure that this method is only called once.
    pub unsafe fn init(&self, value: T) {
        unsafe { *self.value.get() = MaybeUninit::new(value); }
    }

    /// # Safety
    /// The caller must ensure that init() has been called before.
    pub unsafe fn get(&self) -> &T {
        unsafe { (*self.value.get()).assume_init_ref() }
    }

    /// # Safety
    /// The caller must ensure that init() has been called before.
    pub unsafe fn get_mut(&mut self) -> &mut T {
        unsafe { (*self.value.get_mut()).assume_init_mut() }
    }
}
