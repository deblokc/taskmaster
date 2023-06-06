use std::collections::HashMap;

use nix::libc;
use yaml_rust::{yaml::Hash, Yaml};

#[derive(Debug)]
pub enum RestartState {
    ALWAYS,
    ONERROR,
    NEVER,
}

#[derive(Debug)]
pub struct Program {
    pub name: String,
    pub command: String,
    pub args: String,
    pub numprocs: u8,
    pub priority: u16,
    pub autostart: bool,
    pub startsecs: u32,
    pub startretries: u8,
    pub autorestart: RestartState,
    pub exitcodes: Vec<u8>,
    pub stopsignal: i32,
    pub stopwaitsecs: u32,
    pub stdoutlog: bool,
    pub stdoutlogpath: Option<String>,
    pub stderrlog: bool,
    pub stderrlogpath: Option<String>,
    pub env: Option<HashMap<String, String>>,
    pub workingdir: Option<String>,
    pub umask: Option<u16>,
    pub user: Option<String>,
}

impl Program {
    pub fn new(name: &str, command: String) -> Self {
        Program {
            name: String::from(name),
            command,
            args: String::new(),
            numprocs: 1,
            priority: 999,
            autostart: true,
            startsecs: 1,
            startretries: 3,
            autorestart: RestartState::ALWAYS,
            exitcodes: vec![0],
            stopsignal: libc::SIGTERM,
            stopwaitsecs: 3,
            stdoutlog: true,
            stdoutlogpath: None,
            stderrlog: true,
            stderrlogpath: None,
            env: None,
            workingdir: None,
            umask: None,
            user: None,
        }
    }

    pub fn parse_yaml(&mut self, param: &Hash) -> Option<String> {
        if let Some(msg) = self.set_numprocs(param) {
            return Some(msg);
        };
        if let Some(msg) = self.set_priority(param) {
            return Some(msg);
        };
        if let Some(msg) = self.set_autostart(param) {
            return Some(msg);
        };
        if let Some(msg) = self.set_startsecs(param) {
            return Some(msg);
        };
        if let Some(msg) = self.set_startretries(param) {
            return Some(msg);
        };
        if let Some(msg) = self.set_autorestart(param) {
            return Some(msg);
        };
        if let Some(msg) = self.set_exitcodes(param) {
            return Some(msg);
        };
        if let Some(msg) = self.set_stopsignal(param) {
            return Some(msg);
        };
        if let Some(msg) = self.set_stopwaitsecs(param) {
            return Some(msg);
        };
        if let Some(msg) = self.set_stdoutlog(param) {
            return Some(msg);
        };
        if let Some(msg) = self.set_stdoutlogpath(param) {
            return Some(msg);
        };
        if let Some(msg) = self.set_stderrlog(param) {
            return Some(msg);
        };
        if let Some(msg) = self.set_stderrlogpath(param) {
            return Some(msg);
        };
        if let Some(msg) = self.set_env(param) {
            return Some(msg);
        };
        if let Some(msg) = self.set_workingdir(param) {
            return Some(msg);
        };
        if let Some(msg) = self.set_umask(param) {
            return Some(msg);
        };
        if let Some(msg) = self.set_user(param) {
            return Some(msg);
        };
        None
    }

    fn set_numprocs(&mut self, param: &Hash) -> Option<String> {
        let numprocs_yaml = match param.get(&Yaml::String(String::from("numprocs"))) {
            Some(val) => val,
            None => return None,
        };
        match numprocs_yaml.as_i64() {
            Some(n) => {
                if n > 255 {
                    Some(format!("Number of processes in program {} is above 255, found {}", self.name, n))
                }
                else if n < 1 {
                    Some(format!("Number of processes in program {} is negative or null", self.name))
                }
                else {
                    self.numprocs = n as u8;
                    None
                }
            }
            None => Some(format!("Wrong format in the number of processes in program {}, expected value must range between 1 and 255", self.name)),
        }
    }

    fn set_priority(&mut self, param: &Hash) -> Option<String> {
        let priority_yaml = match param.get(&Yaml::String(String::from("priority"))) {
            Some(val) => val,
            None => return None,
        };
        match priority_yaml.as_i64() {
            Some(n) => {
                if n > 999 {
                    Some(format!(
                        "Priority in program {} is above 999, found {}",
                        self.name, n
                    ))
                } else if n < 0 {
                    Some(format!("Priority in program {} is negative", self.name))
                } else {
                    self.priority = n as u16;
                    None
                }
            }
            None => Some(format!(
                "Wrong format priority in program {}, expected value must range between 0 and 999",
                self.name
            )),
        }
    }

    fn set_autostart(&mut self, param: &Hash) -> Option<String> {
        let autostart_yaml = match param.get(&Yaml::String(String::from("autostart"))) {
            Some(val) => val,
            None => return None,
        };
        match autostart_yaml.as_bool() {
            Some(val) => {
                self.autostart = val;
                None
            }
            None => Some(format!(
                "Bad value for autostart in program {} detected, expecting boolean value",
                self.name
            )),
        }
    }

    fn set_startsecs(&mut self, param: &Hash) -> Option<String> {
        let startsecs_yaml = match param.get(&Yaml::String(String::from("startsecs"))) {
            Some(val) => val,
            None => return None,
        };
        match startsecs_yaml.as_i64() {
            Some(n) => {
                if n > 4_294_967_295 {
                    Some(format!("Value provided for number of startsecs in program {} is too high, maximum allowed value is 4 294 967 295, found {}", self.name, n))
                } else if n < 0 {
                    Some(format!(
                        "Value provided for number of startsecs in program {} is negative",
                        self.name
                    ))
                } else {
                    self.startsecs = n as u32;
                    None
                }
            },
            None =>  Some(format!("Value provided for number of startsecs in program {} not a valid signed 64 bits number", self.name))
        }
    }

    fn set_startretries(&mut self, param: &Hash) -> Option<String> {
        let startretries_yaml = match param.get(&Yaml::String(String::from("startretries"))) {
            Some(val) => val,
            None => return None,
        };
        match startretries_yaml.as_i64() {
            Some(n) => {
                if n > 255 {
                    Some(format!("Value provided for number of startretries in program {} is too high, maximum allowed value is 255, found {}", self.name, n))
                } else if n < 0 {
                    Some(format!(
                        "Value provided for number of startretries in program {} is negative",
                        self.name
                    ))
                } else {
                    self.startretries = n as u8;
                    None
                }
            }
            None => Some(format!(
                "Value provided for number of startretries in program {} not a valid number",
                self.name
            )),
        }
    }

    fn set_autorestart(&mut self, param: &Hash) -> Option<String> {
        let autorestart_yaml = match param.get(&Yaml::String(String::from("autorestart"))) {
            Some(val) => val,
            None => return None,
        };
        match autorestart_yaml.as_str() {
            Some(val) => {
                if val == "always" {
                    self.autorestart = RestartState::ALWAYS;
                    None
                } else if val == "onerror" {
                    self.autorestart = RestartState::ONERROR;
                    None
                } else if val == "never" {
                    self.autorestart = RestartState::NEVER;
                    None
                } else {
                    Some(format!("Value provided for autorestart in program {} is invalid, expected values are : \n\t- always\n\t- onerror\n\t- never", self.name))
                }
            }
            None => Some(format!(
                "Value provided for autorestart in program {} is not a valid string",
                self.name
            )),
        }
    }

    fn set_exitcodes(&mut self, param: &Hash) -> Option<String> {
        let exitcodes_vec_yaml = match param.get(&Yaml::String(String::from("exitcodes"))) {
            Some(arr) => arr,
            None => return None,
        };
        let exitcodes_vec = match exitcodes_vec_yaml.as_vec() {
            Some(arr) => arr,
            None => {
                return Some(format!(
                    "Value provided for exitcodes in program {} invalid, expected an array",
                    self.name
                ))
            }
        };
        self.exitcodes.clear();
        for code in exitcodes_vec.iter() {
            match code.as_i64() {
                Some(n) => {
                    if n > 255 {
                        return Some(format!("Value provided for exitcodes in program {} is not a valid number, maximum value is 255, found {}", self.name, n));
                    } else if n < 0 {
                        return Some(format!(
                            "Value provided for exitcodes in program {} is negative",
                            self.name
                        ));
                    } else {
                        let nu8 = n as u8;
                        if !self.exitcodes.contains(&nu8) {
                            self.exitcodes.push(nu8)
                        }
                    }
                }
                None => {
                    return Some(format!(
                        "A value provided for exitcodes in program {} is not a valid number",
                        self.name
                    ))
                }
            };
        }
        None
    }

    fn set_stopsignal(&mut self, param: &Hash) -> Option<String> {
        let stopsignal_yaml = match param.get(&Yaml::String(String::from("stopsignal"))) {
            Some(val) => val,
            None => return None,
        };
        match stopsignal_yaml.as_str() {
            Some(val) => {
                if val == "HUP" {
                    self.stopsignal = libc::SIGHUP;
                    None
                } else if val == "USR1" {
                    self.stopsignal = libc::SIGUSR1;
                    None
                } else if val == "USR2" {
                    self.stopsignal = libc::SIGUSR2;
                    None
                } else if val == "TERM" {
                    self.stopsignal = libc::SIGTERM;
                    None
                } else if val == "INT" {
                    self.stopsignal = libc::SIGINT;
                    None
                } else if val == "QUIT" {
                    self.stopsignal = libc::SIGQUIT;
                    None
                } else if val == "KILL" {
                    self.stopsignal = libc::SIGKILL;
                    None
                } else {
                    Some(format!("Value provided for autorestart in program {} is invalid, expected values are : \n\t- TERM\n\t- HUP\n\t- INT\n\t- QUIT\n\t- KILL\n\t- USR1\n\t- USR2", self.name))
                }
            }
            None => Some(format!(
                "Value provided for autorestart in program {} is not a valid string",
                self.name
            )),
        }
    }

    fn set_stopwaitsecs(&mut self, param: &Hash) -> Option<String> {
        let stopwaitsecs_yaml = match param.get(&Yaml::String(String::from("stopwaitsecs"))) {
            Some(val) => val,
            None => return None,
        };
        match stopwaitsecs_yaml.as_i64() {
            Some(n) => {
                if n > 4_294_967_295 {
                    Some(format!("Value provided for number of stopwaitsecs in program {} is too high, maximum allowed value is 4 294 967 295, found {}", self.name, n))
                } else if n < 0 {
                    Some(format!(
                        "Value provided for number of stopwaitsecs in program {} is negative",
                        self.name
                    ))
                } else {
                    self.stopwaitsecs = n as u32;
                    None
                }
            },
            None =>  Some(format!("Value provided for number of stopwaitsecs in program {} not a valid signed 64 bits number", self.name))
        }
    }

    fn set_stdoutlog(&mut self, param: &Hash) -> Option<String> {
        let stdoutlog_yaml = match param.get(&Yaml::String(String::from("stdoutlog"))) {
            Some(val) => val,
            None => return None,
        };
        match stdoutlog_yaml.as_bool() {
            Some(val) => {
                self.stdoutlog = val;
                None
            }
            None => Some(format!(
                "Bad value for stdoutlog in program {} detected, expecting boolean value",
                self.name
            )),
        }
    }

    fn set_stdoutlogpath(&mut self, param: &Hash) -> Option<String> {
        let stdoutlogpath_yaml = match param.get(&Yaml::String(String::from("stdoutlogpath"))) {
            Some(val) => val,
            None => return None,
        };
        match stdoutlogpath_yaml.as_str() {
            Some(val) => {
                self.stdoutlogpath = Some(String::from(val));
                None
            }
            None => Some(format!(
                "Bad value for stdoutlogpath in program {} detected, expecting a string",
                self.name
            )),
        }
    }

    fn set_stderrlog(&mut self, param: &Hash) -> Option<String> {
        let stderrlog_yaml = match param.get(&Yaml::String(String::from("stderrlog"))) {
            Some(val) => val,
            None => return None,
        };
        match stderrlog_yaml.as_bool() {
            Some(val) => {
                self.stderrlog = val;
                None
            }
            None => Some(format!(
                "Bad value for stderrlog in program {} detected, expecting boolean value",
                self.name
            )),
        }
    }

    fn set_stderrlogpath(&mut self, param: &Hash) -> Option<String> {
        let stderrlogpath_yaml = match param.get(&Yaml::String(String::from("stderrlogpath"))) {
            Some(val) => val,
            None => return None,
        };
        match stderrlogpath_yaml.as_str() {
            Some(val) => {
                self.stderrlogpath = Some(String::from(val));
                None
            }
            None => Some(format!(
                "Bad value for stderrlogpath in program {} detected, expecting a string",
                self.name
            )),
        }
    }

    fn set_env(&mut self, param: &Hash) -> Option<String> {
        let env_hash_yaml = match param.get(&Yaml::String(String::from("env"))) {
            Some(arr) => arr,
            None => return None,
        };
        let env_hash = match env_hash_yaml.as_hash() {
            Some(map) => map,
            None => {
                return Some(format!(
                    "Value provided for env in program {} is invalid, expected a map",
                    self.name
                ))
            }
        };
        if env_hash.is_empty() {
            return None;
        }
        let mut new_env: HashMap<String, String> = HashMap::new();
        for (env_key, env_val) in env_hash.iter() {
            let key = match env_key.as_str() {
                Some(k) => String::from(k),
                None => {
                    return Some(format!(
                        "A key provided for env in program {} is not a valid string",
                        self.name
                    ))
                }
            };
            let val = match env_val.as_str() {
                Some(v) => String::from(v),
                None => {
                    return Some(format!(
                        "Value provided for key {} in the env of program {} is not a valid string",
                        key, self.name
                    ))
                }
            };
            new_env.insert(key, val);
        }
        self.env = Some(new_env);
        None
    }

    fn set_workingdir(&mut self, param: &Hash) -> Option<String> {
        let workingdir_yaml = match param.get(&Yaml::String(String::from("workingdir"))) {
            Some(val) => val,
            None => return None,
        };
        match workingdir_yaml.as_str() {
            Some(val) => {
                self.workingdir = Some(String::from(val));
                None
            }
            None => Some(format!(
                "Bad value for workingdir in program {} detected, expecting a string",
                self.name
            )),
        }
    }

    fn set_umask(&mut self, param: &Hash) -> Option<String> {
        let umask_yaml = match param.get(&Yaml::String(String::from("umask"))) {
            Some(val) => val,
            None => return None,
        };
        match umask_yaml.as_i64() {
            Some(n) => {
                if n > 511 {
                    Some(format!(
                        "Umask in program {} is not a valid octal umask, found {:#o}",
                        self.name, n
                    ))
                } else if n < 0 {
                    Some(format!("Umask in program {} is negative", self.name))
                } else {
                    self.umask = Some(n as u16);
                    None
                }
            }
            None => Some(format!(
                "Wrong format for umask in program {}, expected value must ba a valid octal number between 0o000 and 0o777",
                self.name
            )),
        }
    }

    fn set_user(&mut self, param: &Hash) -> Option<String> {
        let user_yaml = match param.get(&Yaml::String(String::from("user"))) {
            Some(val) => val,
            None => return None,
        };
        match user_yaml.as_str() {
            Some(val) => {
                self.user = Some(String::from(val));
                None
            }
            None => Some(format!(
                "Bad value for user in program {} detected, expecting a string",
                self.name
            )),
        }
    }
}
