fn main() {
    let platform = std::env::var("PLATFORM").expect("PLATFORM is not set");
    println!("cargo:rerun-if-env-changed=PLATFORM");

    opal_build::NasmBuild::new("src")
        .files_under_exclude(".", ["platform"])
        .files_under(format!("platform/{platform}"))
        .compile("opal_kernel_asm");
}
