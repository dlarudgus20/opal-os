use std::collections::BTreeSet;
use std::env;
use std::fs;
use std::path::{Component, Path, PathBuf};
use std::process::Command;

pub struct NasmBuild {
    root: PathBuf,
    sources: BTreeSet<AsmSource>,
    source_dirs: Vec<RelPath>,
    format: &'static str,
}

impl NasmBuild {
    pub fn new<P: AsRef<Path>>(root: P) -> Self {
        let root = RelPath::parse(root);
        let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
        let root = manifest_dir.join(root.as_path());

        Self {
            root,
            sources: BTreeSet::new(),
            source_dirs: Vec::new(),
            format: "elf64",
        }
    }

    pub fn files_under<P: AsRef<Path>>(self, dir: P) -> Self {
        self.files_under_exclude(dir, std::iter::empty::<&str>())
    }

    pub fn files_under_exclude<P, I, E>(mut self, dir: P, excludes: I) -> Self
    where
        P: AsRef<Path>,
        I: IntoIterator<Item = E>,
        E: AsRef<Path>,
    {
        let dir = RelPath::parse(dir);
        let excludes = excludes.into_iter().map(RelPath::parse).collect::<Vec<_>>();
        let source_dir = self.root.join(dir.as_path());

        println!("cargo:rerun-if-changed={}", source_dir.display());
        collect_asm_files(
            &self.root,
            &source_dir,
            &source_dir,
            &excludes,
            &mut self.sources,
        );
        self.source_dirs.push(dir);
        self
    }

    pub fn compile(self, name: &str) {
        assert!(
            !self.sources.is_empty(),
            "no assembly sources found under {}",
            self.source_dirs
                .iter()
                .map(|path| path.as_path().display().to_string())
                .collect::<Vec<_>>()
                .join(", ")
        );

        let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
        let archive = out_dir.join(format!("lib{name}.a"));
        let nasm = env::var("TOOLSET_NASM").unwrap_or_else(|_| "nasm".to_string());
        let ar = env::var("TOOLSET_AR").unwrap_or_else(|_| "ar".to_string());

        let mut objects = Vec::new();
        for source in &self.sources {
            println!("cargo:rerun-if-changed={}", source.path.display());

            let mut obj = out_dir.join(&source.relative_path).into_os_string();
            obj.push(".o");
            let obj = PathBuf::from(obj);
            fs::create_dir_all(obj.parent().unwrap()).unwrap_or_else(|err| {
                panic!("failed to create directory for {}: {err}", obj.display());
            });

            let mut nasm_cmd = Command::new(&nasm);
            nasm_cmd
                .arg("-f")
                .arg(self.format)
                .arg(&source.path)
                .arg("-o")
                .arg(&obj);
            run(nasm_cmd);
            objects.push(obj);
        }

        let mut ar_cmd = Command::new(ar);
        ar_cmd.arg("crs").arg(&archive).args(&objects);
        run(ar_cmd);

        println!("cargo:rustc-link-search=native={}", out_dir.display());
        println!("cargo:rustc-link-lib=static={name}");
    }
}

#[derive(Eq, Ord, PartialEq, PartialOrd)]
struct AsmSource {
    path: PathBuf,
    relative_path: PathBuf,
}

struct RelPath {
    path: PathBuf,
}

impl RelPath {
    fn parse<P: AsRef<Path>>(path: P) -> Self {
        let path = path.as_ref();
        assert!(!path.as_os_str().is_empty(), "path must not be empty");

        if path == Path::new(".") {
            return Self {
                path: PathBuf::new(),
            };
        }

        let path_text = path.to_str().expect("path must be valid UTF-8");
        assert!(
            !path_text.split('/').any(|component| component == "."),
            "'.' is only allowed as the whole path: {}",
            path.display()
        );

        let mut out = PathBuf::new();
        for component in path.components() {
            match component {
                Component::Normal(part) => out.push(part),
                Component::CurDir => {
                    panic!("'.' is only allowed as the whole path: {}", path.display());
                }
                Component::ParentDir | Component::RootDir | Component::Prefix(_) => {
                    panic!(
                        "path must be root-relative without '..': {}",
                        path.display()
                    );
                }
            }
        }

        Self { path: out }
    }

    fn as_path(&self) -> &Path {
        &self.path
    }
}

fn collect_asm_files(
    root_dir: &Path,
    collect_dir: &Path,
    dir: &Path,
    excludes: &[RelPath],
    files: &mut BTreeSet<AsmSource>,
) {
    let entries = fs::read_dir(dir).unwrap_or_else(|err| {
        panic!("failed to read directory {}: {err}", dir.display());
    });

    for entry in entries {
        let entry = entry.unwrap_or_else(|err| {
            panic!("failed to read entry under {}: {err}", dir.display());
        });
        let path = entry.path();
        let file_type = entry.file_type().unwrap_or_else(|err| {
            panic!("failed to read file type for {}: {err}", path.display());
        });

        if is_excluded(collect_dir, &path, excludes) {
            continue;
        }

        if file_type.is_dir() {
            collect_asm_files(root_dir, collect_dir, &path, excludes, files);
        } else if file_type.is_file()
            && path.extension().and_then(|ext| ext.to_str()) == Some("asm")
        {
            let relative_path = path.strip_prefix(root_dir).unwrap_or_else(|err| {
                panic!(
                    "failed to make {} relative to {}: {err}",
                    path.display(),
                    root_dir.display()
                );
            });
            let relative_path = relative_path.to_path_buf();
            files.insert(AsmSource {
                path,
                relative_path,
            });
        }
    }
}

fn is_excluded(collect_dir: &Path, path: &Path, excludes: &[RelPath]) -> bool {
    let relative_path = path.strip_prefix(collect_dir).unwrap_or_else(|err| {
        panic!(
            "failed to make {} relative to {}: {err}",
            path.display(),
            collect_dir.display()
        );
    });

    excludes
        .iter()
        .any(|exclude| relative_path.starts_with(exclude.as_path()))
}

fn run(mut cmd: Command) {
    let status = cmd.status().expect("failed to spawn command");
    assert!(status.success(), "command failed: {cmd:?}");
}
