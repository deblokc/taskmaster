extern crate yaml_rust;
use std::{collections::HashMap, fs::File, io::Read};
// use std::io;
use program::Program;
use std::sync::{Arc, Mutex};
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

pub fn get_programs(map: &Hash) -> Result<HashMap<String, Arc<Mutex<Program>>>, String> {
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
    let mut ret_map: HashMap<String, Arc<Mutex<Program>>> = HashMap::new();
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
        let prog = Program::create(&namep, param_unwrapped)?;
        ret_map.insert(namep, Arc::new(Mutex::new(prog)));
    }
    Ok(ret_map)
}

pub fn order_priorities(
    program_list: &HashMap<String, Arc<Mutex<Program>>>,
) -> HashMap<u16, Vec<Arc<Mutex<Program>>>> {
    let mut priorities: HashMap<u16, Vec<Arc<Mutex<Program>>>> = HashMap::new();
    for (_program_name, program) in program_list.iter() {
        let prog = program.lock().expect("Cannot acquire mutex");
        if let Some(val) = priorities.get_mut(&(prog.priority)) {
            val.push(Arc::clone(program));
        } else {
            priorities.insert(prog.priority, vec![Arc::clone(program)]);
        }
    }
    priorities
}
