extern crate yaml_rust;
use std::{fs::File, io::Read};
// use std::io;
// use yaml_rust::{YamlEmitter, YamlLoader};

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
