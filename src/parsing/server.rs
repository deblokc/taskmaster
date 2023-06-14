mod program;
mod sockets;

pub use program::Program;
pub use sockets::UnixSocket;
use std::collections::HashMap;
use yaml_rust::{yaml::Hash, Yaml};

type program_list = Arc<Mutex<HashMap<String, Arc<Mutex<Program>>>>>;

#[derive(Debug)]
pub struct Server {
    pub unixsocket: Option<UnixSocket>,
    pub programs: program_list,
}

impl Server {
    fn new() -> Self {
        Server {
            unixsocket: None,
            programs: Server::get_programs(map)?,
        }
    }

    pub fn create(map: &Hash) -> Result<Self, String> {
        let server_yaml = match map.get(&Yaml::String(String::from("server"))) {
            None => return Ok(Server::new()),
            Some(server) => server,
        };
        let server = match server_yaml.as_hash() {
            Some(serv) => serv,
            None => {
                return Err(format!(
                    "Server block has an incorrect format, expected map"
                ))
            }
        };
        Ok(Server {
            unixsocket: UnixSocket::create(server)?,
            programs: Server::get_programs(map)?,
        })
    }

    fn get_programs(
        map: &Hash,
    ) -> Result<Arc<Mutex<HashMap<String, Arc<Mutex<Program>>>>>, String> {
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
        Ok(Arc::new(Mutex::new(ret_map)))
    }
}
