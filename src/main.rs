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
    let programs = match parsing::get_programs(&parsed) {
        Ok(p) => p,
        Err(msg) => {
            eprintln!("{msg}");
            return;
        }
    };
    let priorities = parsing::order_priorities(&programs);
    print_programs(&programs);
    println!("\n\nPriorities\n{priorities:?}");
    println!("End");
}

fn print_programs(programs: &HashMap<String, Program>) {
    eprintln!("\n\n\t\t*********   PROGRAMS   *********\n");
    for (name, program) in programs.iter() {
        eprintln!("program {name}:\n\t{program:?}");
    }
}
