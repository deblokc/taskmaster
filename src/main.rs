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
    println!("Hello, world!");
}
