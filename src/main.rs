use std::{
    collections::HashMap,
    env,
    sync::{Arc, Mutex},
};
use taskmaster::parsing::{self, program::Program};
use taskmaster::process::{self};

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
        Ok(p) => Mutex::new(p),
        Err(msg) => {
            eprintln!("{msg}");
            return;
        }
    };
    let prog_map = programs.lock().expect("Cannot acquire mutex");
    let priorities: HashMap<u16, Vec<Arc<Mutex<Program>>>> = parsing::order_priorities(&prog_map);
    print_programs(&prog_map);
    println!("\n\nPriorities\n{priorities:?}");
    process::infinity(&priorities);
    println!("End");
}

fn print_programs(programs: &HashMap<String, Arc<Mutex<Program>>>) {
    eprintln!("\n\n\t\t*********   PROGRAMS   *********\n");
    for (name, program) in programs.iter() {
        eprintln!("program {name}:\n\t{program:?}");
    }
}
