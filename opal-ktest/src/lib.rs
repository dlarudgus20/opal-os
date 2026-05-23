use proc_macro::{Delimiter, Punct, Spacing, TokenStream, TokenTree};

#[proc_macro_attribute]
pub fn ktest(attr: TokenStream, item: TokenStream) -> TokenStream {
    if !attr.is_empty() {
        return compile_error("#[ktest] does not accept arguments");
    }

    let name = match find_test_fn_name(&item) {
        Ok(name) => name,
        Err(TestFnError::HasReturnType) => {
            return compile_error("#[ktest] must be applied to a fn() without return type");
        }
        Err(TestFnError::InvalidSignature) => {
            return compile_error("#[ktest] must be applied to a non-generic fn()");
        }
    };

    let static_name = format!("__OPAL_KTEST_{}", name);
    let run_name = format!("__opal_ktest_run_{}", name);
    let expanded = format!(
        r#"
#[cfg(opal_kernel_test)]
{item}

#[cfg(opal_kernel_test)]
extern "C" fn {run_name}() {{
    {name}();
}}

#[cfg(opal_kernel_test)]
#[used]
#[unsafe(link_section = ".ktest")]
static {static_name}: ::opal_kernel::ktest::KernelTest = ::opal_kernel::ktest::KernelTest {{
    name_ptr: "{name}".as_ptr(),
    name_len: "{name}".len(),
    func: {run_name},
}};
"#,
        item = item,
        run_name = run_name,
        static_name = static_name,
        name = name,
    );

    expanded.parse().expect("failed to generate #[ktest]")
}

enum TestFnError {
    InvalidSignature,
    HasReturnType,
}

fn find_test_fn_name(item: &TokenStream) -> Result<String, TestFnError> {
    let mut iter = item.clone().into_iter();

    while let Some(token) = iter.next() {
        let TokenTree::Ident(ident) = token else {
            continue;
        };
        if ident.to_string() != "fn" {
            continue;
        }

        let Some(TokenTree::Ident(name)) = iter.next() else {
            return Err(TestFnError::InvalidSignature);
        };

        let Some(TokenTree::Group(args)) = iter.next() else {
            return Err(TestFnError::InvalidSignature);
        };
        if args.delimiter() != Delimiter::Parenthesis || !args.stream().is_empty() {
            return Err(TestFnError::InvalidSignature);
        }

        if has_return_type(iter) {
            return Err(TestFnError::HasReturnType);
        }

        return Ok(name.to_string());
    }

    Err(TestFnError::InvalidSignature)
}

fn has_return_type(mut iter: impl Iterator<Item = TokenTree>) -> bool {
    while let Some(token) = iter.next() {
        match token {
            TokenTree::Punct(punct) if is_return_arrow_start(&punct) => {
                return matches!(
                    iter.next(),
                    Some(TokenTree::Punct(next)) if next.as_char() == '>'
                );
            }
            TokenTree::Group(group) if group.delimiter() == Delimiter::Brace => return false,
            _ => {}
        }
    }

    false
}

fn is_return_arrow_start(punct: &Punct) -> bool {
    punct.as_char() == '-' && punct.spacing() == Spacing::Joint
}

fn compile_error(message: &str) -> TokenStream {
    format!("compile_error!({message:?});")
        .parse()
        .expect("failed to generate compile_error")
}
