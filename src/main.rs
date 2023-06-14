use std::{
    collections::HashMap,
    env,
    sync::{Arc, Mutex},
};

use taskmaster::process::{self};
use taskmaster::{parsing, Program, Server};

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
    let server = match Server::create(&parsed) {
        Ok(val) => val,
        Err(msg) => {
            eprintln!("{msg}");
            return;
        }
    };
    let prog_map = server.programs.lock().expect("Cannot acquire mutex");
    let priorities: HashMap<u16, Vec<Arc<Mutex<Program>>>> = parsing::order_priorities(&prog_map);
    print_programs(&prog_map);
    drop(prog_map);
    println!("\n\nPriorities\n{priorities:?}");
    println!("\n\nServer\n{server:?}");
    process::infinity(&priorities);
    println!("End");
}

fn print_programs(programs: &HashMap<String, Arc<Mutex<Program>>>) {
    eprintln!("\n\n\t\t*********   PROGRAMS   *********\n");
    for (name, program) in programs.iter() {
        eprintln!("program {name}:\n\t{program:?}");
    }
}
