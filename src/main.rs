use std::{collections::HashMap, env};
use taskmaster::parsing::{self, program::Program};

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
    let mut priorities: HashMap<u16, Vec<&Program>> = HashMap::new();
    let (mut _priority_list, mut programs, err) = parsing::get_programs(&parsed, &mut priorities);
    if let Some(msg) = err {
        eprintln!("{msg}");
        return;
    }
    println!("Parsed: {:?}", parsed);
    eprintln!("Mapped: {:?}", programs);
    println!("End");
}
