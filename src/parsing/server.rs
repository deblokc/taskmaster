mod log;
mod program;
mod sockets;

pub use log::Log;
pub use program::Program;
pub use program::RestartState;
pub use sockets::UnixSocket;
use std::{
    collections::HashMap,
    sync::{Arc, Mutex},
};
use yaml_rust::{yaml::Hash, Yaml};

pub type ProgramMap = Arc<Mutex<HashMap<String, Arc<Mutex<Program>>>>>;

#[derive(Debug)]
pub struct Server {
    pub programs: ProgramMap,
    pub unixsocket: Option<UnixSocket>,
    pub logfile: String,
    pub logfile_maxbytes: u64,
    pub logfile_backups: u8,
    pub loglevel: Log,
}

impl Server {
    fn default(map: &Hash) -> Result<Self, String> {
        Ok(Server {
            programs: Server::get_programs(map)?,
            unixsocket: None,
            logfile: String::from("taskmaster.log"),
            logfile_maxbytes: (5 * 1024 * 1024),
            logfile_backups: 5,
            loglevel: Log::INFO,
        })
    }

    pub fn create(map: &Hash) -> Result<Self, String> {
        let server_yaml = match map.get(&Yaml::String(String::from("server"))) {
            None => return Server::default(map),
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
            programs: Server::get_programs(map)?,
            unixsocket: UnixSocket::create(server)?,
            logfile: Server::set_logfile(server)?,
            logfile_maxbytes: Server::set_logfile_maxbytes(server)?,
            logfile_backups: Server::set_logfile_backups(server)?,
            loglevel: Server::set_loglevel(server)?,
        })
    }

    fn set_logfile(server: &Hash) -> Result<String, String> {
        let logfile = match server.get(&Yaml::String(String::from("logfile"))) {
            None => return Ok(String::from("taskmaster.log")),
            Some(val) => val,
        };
        match logfile.as_str() {
            None => Err(format!("Error in server block, logfile is not a String")),
            Some(val) => Ok(val.to_string()),
        }
    }

    fn set_logfile_maxbytes(server: &Hash) -> Result<u64, String> {
        let logfile_maxbytes = match server.get(&Yaml::String(String::from("logfile_maxbytes"))) {
            None => return Ok(5 * 1024 * 1024),
            Some(val) => val,
        };
        match logfile_maxbytes.as_i64() {
            None => Err(format!(
                "Error in server block, logfile_maxbytes is not a number"
            )),
            Some(val) => {
                if val < 0 {
                    Err(format!(
                        "Error in server block, logfile_maxbytes must be a positive number"
                    ))
                } else {
                    Ok(val as u64)
                }
            }
        }
    }

    fn set_logfile_backups(server: &Hash) -> Result<u8, String> {
        let logfile_maxbytes = match server.get(&Yaml::String(String::from("logfile_backups"))) {
            None => return Ok(5),
            Some(val) => val,
        };
        match logfile_maxbytes.as_i64() {
            None => Err(format!(
                "Error in server block, logfile_backups is not a number"
            )),
            Some(val) => {
                if val < 0 {
                    Err(format!(
                        "Error in server block, logfile_backups must be a positive number"
                    ))
                } else if val > 255 {
                    Err(format!(
                        "Error in server block, logfile_backups cannot be over 255, found {val}"
                    ))
                } else {
                    Ok(val as u8)
                }
            }
        }
    }

    fn set_loglevel(server: &Hash) -> Result<Log, String> {
        let loglevel = match server.get(&Yaml::String(String::from("loglevel"))) {
            None => return Ok(Log::INFO),
            Some(val) => val,
        };
        match loglevel.as_str() {
            None => Err(format!("Error in server block, loglevel is not a String")),
            Some(val) => {
                if val == "CRIT" {
                    Ok(Log::CRIT)
                } else if val == "ERRO" {
                    Ok(Log::ERRO)
                } else if val == "INFO" {
                    Ok(Log::INFO)
                } else if val == "DEBG" {
                    Ok(Log::DEBG)
                } else {
                    Err(format!("Value provided for loglevel is invalid, expected values are : \n\t- CRIT\n\t- ERRO\n\t- INFO\n\t- DEBG"))
                }
            }
        }
    }

    fn get_programs(map: &Hash) -> Result<ProgramMap, String> {
        let programs = match map.get(&Yaml::String(String::from("programs"))) {
            None => return Ok(Arc::new(Mutex::new(HashMap::new()))),
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
