extern crate yaml_rust;
use std::{collections::HashMap, fs::File, io::Read};
// use std::io;
use program::Program;
use yaml_rust::{yaml::Hash, YamlLoader};

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

pub fn get_programs(
    map: Hash,
    priorities: &mut HashMap<u16, Vec<&Program>>,
) -> (Vec<u16>, HashMap<String, Program>) {
    todo!();
}
