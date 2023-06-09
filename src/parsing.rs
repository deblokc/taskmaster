extern crate yaml_rust;
use std::{collections::HashMap, fs::File, io::Read};
// use std::io;
pub use program::Program;
use yaml_rust::{yaml::Hash, Yaml, YamlLoader};

pub mod program;

pub fn get_file_content(filename: Option<String>) -> Result<String, String> {
    let filename = match filename {
        Some(file) => file,
        None => return Err(String::from("No file passed as parameter")),
    };

    let mut buffer = String::new();
    let mut fs = match File::open(filename) {
        Ok(f) => f,
        Err(e) => return Err(format!("Error opening file: {e}")),
    };
    if let Err(e) = fs.read_to_string(&mut buffer) {
        return Err(format!("Error reading file content: {e}"));
    };
    Ok(buffer)
}

pub fn load_yaml(source: &str) -> Result<Hash, String> {
    let mut config = match YamlLoader::load_from_str(source) {
        Ok(t) => t,
        Err(e) => return Err(format!("Error loading configuration file: {e}")),
    };
    if config.len() == 0 {
        Ok(Hash::new())
    } else if config.len() != 1 {
        Err(format!(
            "Incorrect formatting, please verify the supplied configuration file"
        ))
    } else {
        match config
            .pop()
            .expect("Oh my god, the vector emptied itself")
            .into_hash()
        {
            Some(h) => Ok(h),
            None => Err(format!(
                "Incorrect formatting, please verify the supplied configuration file"
            )),
        }
    }
}

fn parse_program(name: &str, param: &Hash) -> Result<Program, String> {
    let command_yaml = match param.get(&Yaml::String(String::from("command"))) {
        Some(c) => c,
        None => return Err(format!("No command found for program {name}")),
    };
    let command = match command_yaml.as_str() {
        Some(c) => String::from(c),
        None => return Err(format!("Command is not a string in program {name}")),
    };

    let mut program = Program::new(name, command);
    if let Some(msg) = program.parse_yaml(param) {
        return Err(msg);
    };
    Ok(program)
}

pub fn get_programs(map: &Hash) -> Result<HashMap<String, Program>, String> {
    let programs = match map.get(&Yaml::String(String::from("programs"))) {
        None => return Ok(HashMap::new()),
        Some(progs) => progs,
    };
    let programs_map = match programs.as_hash() {
        None => {
            return Err(format!(
                "Programs must be provided as a map, please see configuration examples"
            ))
        }
        Some(m) => m,
    };
    let mut ret_map: HashMap<String, Program> = HashMap::new();
    for (name, param) in programs_map.iter() {
        let namep = match name.as_str() {
            None => return Err(format!("Invalid program name detected")),
            Some(n) => String::from(n),
        };
        let param_unwrapped = match param.as_hash() {
            None => {
                return Err(format!(
                    "Program parameters in program {namep} incorrectly formatted, map expected"
                ))
            }
            Some(n) => n,
        };
        let program = parse_program(&namep, param_unwrapped)?;
        ret_map.insert(namep, program);
    }
    Ok(ret_map)
}

pub fn order_priorities(program_list: &HashMap<String, Program>) -> HashMap<u16, Vec<&Program>> {
    let mut priorities: HashMap<u16, Vec<&Program>> = HashMap::new();
    for (_program_name, program) in program_list.iter() {
        if let Some(val) = priorities.get_mut(&program.priority) {
            val.push(program);
        } else {
            priorities.insert(program.priority, vec![program]);
        }
    }
    priorities
}
