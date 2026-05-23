#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [[ -n "${RUSTC:-}" ]]; then
    read -r -a rustc_cmd <<< "$RUSTC"
else
    rustc_cmd=(rustc +"${RUST_TOOLCHAIN:-nightly-2026-04-14}")
fi

rust_target="${RUST_TARGET_NAME:-x86_64-unknown-none}"
rust_edition="${RUST_EDITION:-2024}"
sysroot="$("${rustc_cmd[@]}" --print sysroot)"
sysroot_src="$sysroot/lib/rustlib/src/rust/library"
ktest_lib="opal-ktest/target/debug/libopal_ktest.so"

if [[ ! -d "$sysroot_src" ]]; then
    echo "missing Rust source: run 'rustup component add rust-src --toolchain ${RUST_TOOLCHAIN:-nightly-2026-04-14}'" >&2
    exit 1
fi

if [[ ! -f "$ktest_lib" ]]; then
    echo "missing proc macro dylib: run 'make -C opal-ktest build'" >&2
    exit 1
fi

cat > rust-project.json <<EOF
{
    "sysroot": "$sysroot",
    "sysroot_src": "$sysroot_src",
    "crates": [
        {
            "display_name": "opal-kernel",
            "root_module": "opal-kernel/src/lib.rs",
            "edition": "$rust_edition",
            "deps": [
                {
                    "crate": 1,
                    "name": "opal_ktest"
                }
            ],
            "cfg": [
                "opal_ktest"
            ],
            "target": "$rust_target",
            "is_workspace_member": true,
            "source": {
                "include_dirs": [
                    "opal-kernel/src"
                ],
                "exclude_dirs": [
                    "opal-kernel/build"
                ]
            }
        },
        {
            "display_name": "opal-ktest",
            "root_module": "opal-ktest/src/lib.rs",
            "edition": "$rust_edition",
            "deps": [],
            "is_workspace_member": true,
            "is_proc_macro": true,
            "proc_macro_dylib_path": "$ktest_lib",
            "source": {
                "include_dirs": [
                    "opal-ktest/src"
                ],
                "exclude_dirs": [
                    "opal-ktest/build"
                ]
            }
        }
    ]
}
EOF
