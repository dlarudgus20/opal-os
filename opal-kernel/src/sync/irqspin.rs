use core::marker::PhantomData;
use core::ops::{Deref, DerefMut};

use crate::arch;
use crate::sync::spin;

pub struct Mutex<T: ?Sized> {
    lock: spin::Mutex<T>,
}

unsafe impl<T: ?Sized + Send> Send for Mutex<T> {}
unsafe impl<T: ?Sized + Send> Sync for Mutex<T> {}

impl<T> Mutex<T> {
    pub const fn new(data: T) -> Self {
        Self {
            lock: spin::Mutex::new(data),
        }
    }

    pub fn into_inner(self) -> T {
        self.lock.into_inner()
    }
}

impl<T: ?Sized> Mutex<T> {
    pub fn lock(&self) -> MutexGuard<'_, T> {
        let was_enabled = arch::interrupts_enabled();
        arch::disable_interrupts();

        MutexGuard {
            guard: Some(self.lock.lock()),
            was_enabled,
            _not_send: PhantomData,
        }
    }

    pub fn try_lock(&self) -> Option<MutexGuard<'_, T>> {
        let was_enabled = arch::interrupts_enabled();
        arch::disable_interrupts();

        match self.lock.try_lock() {
            Some(guard) => Some(MutexGuard {
                guard: Some(guard),
                was_enabled,
                _not_send: PhantomData,
            }),
            None => {
                if was_enabled {
                    arch::enable_interrupts();
                }
                None
            }
        }
    }

    pub fn is_locked(&self) -> bool {
        self.lock.is_locked()
    }

    pub fn get_mut(&mut self) -> &mut T {
        self.lock.get_mut()
    }
}

pub struct MutexGuard<'a, T: ?Sized> {
    guard: Option<spin::MutexGuard<'a, T>>,
    was_enabled: bool,
    _not_send: PhantomData<*mut ()>,
}

impl<T: ?Sized> Deref for MutexGuard<'_, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.guard.as_ref().unwrap()
    }
}

impl<T: ?Sized> DerefMut for MutexGuard<'_, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.guard.as_mut().unwrap()
    }
}

impl<T: ?Sized> Drop for MutexGuard<'_, T> {
    fn drop(&mut self) {
        drop(self.guard.take());

        if self.was_enabled {
            arch::enable_interrupts();
        }
    }
}
