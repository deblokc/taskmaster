use std::env;
use taskmaster::parsing;

fn main() {
    let mut args = env::args();
    args.next();
    let content = match parsing::get_file_content(args.next()) {
        Err(msg) => {
            eprintln!("{msg}");
            return;
        }
        Ok(ct) => ct,
    };
    println!("File content:\n{content}");
    let parsed = match parsing::load_yaml(&content) {
        Err(msg) => {
            eprintln!("{msg}");
            return;
        }
        Ok(c) => c,
    };
    if parsed.contains_key("programs") {
        println!("We have programs ladies and gents");
    }
    println!("Parsed: {:?}", parsed);
    println!("End");
}
