#![no_std]
#![no_main]
#![cfg_attr(test, feature(custom_test_frameworks))]
#![cfg_attr(test, test_runner(opal_kernel::test_runner))]

extern crate opal_kernel;
