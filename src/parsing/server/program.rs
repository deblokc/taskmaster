use nix::libc;
use shlex;
use std::collections::HashMap;
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
    pub args: Option<Vec<String>>,
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
    pub fn create(name: &str, param: &Hash) -> Result<Self, String> {
        let (command, args) = Program::set_command(param, name)?;
        Ok(Program {
            name: String::from(name),
            command,
            args,
            numprocs: Program::set_numprocs(param, name)?,
            priority: Program::set_priority(param, name)?,
            autostart: Program::set_autostart(param, name)?,
            startsecs: Program::set_startsecs(param, name)?,
            startretries: Program::set_startretries(param, name)?,
            autorestart: Program::set_autorestart(param, name)?,
            exitcodes: Program::set_exitcodes(param, name)?,
            stopsignal: Program::set_stopsignal(param, name)?,
            stopwaitsecs: Program::set_stopwaitsecs(param, name)?,
            stdoutlog: Program::set_stdoutlog(param, name)?,
            stdoutlogpath: Program::set_stdoutlogpath(param, name)?,
            stderrlog: Program::set_stderrlog(param, name)?,
            stderrlogpath: Program::set_stderrlogpath(param, name)?,
            env: Program::set_env(param, name)?,
            workingdir: Program::set_workingdir(param, name)?,
            umask: Program::set_umask(param, name)?,
            user: Program::set_user(param, name)?,
        })
    }

    fn set_command(param: &Hash, name: &str) -> Result<(String, Option<Vec<String>>), String> {
        let command_yaml = match param.get(&Yaml::String(String::from("command"))) {
            Some(c) => c,
            None => return Err(format!("No command found for program {}", name)),
        };
        let command = match command_yaml.as_str() {
            Some(c) => String::from(c),
            None => return Err(format!("Command is not a string in program {}", name)),
        };
        let mut args = match shlex::split(&command) {
            Some(args) => args,
            None => return Err(format!("Wrong command formatting in program {}", name)),
        };
        if args.len() == 0 {
            return Err(format!("Empty command in program {}", name));
        };
        let command = args.remove(0);
        if args.len() == 0 {
            Ok((command, None))
        } else {
            Ok((command, Some(args)))
        }
    }

    fn set_numprocs(param: &Hash, name: &str) -> Result<u8, String> {
        let numprocs_yaml = match param.get(&Yaml::String(String::from("numprocs"))) {
            Some(val) => val,
            None => return Ok(1),
        };
        match numprocs_yaml.as_i64() {
            Some(n) => {
                if n > 255 {
                    Err(format!("Number of processes in program {} is above 255, found {}", name, n))
                }
                else if n < 1 {
                    Err(format!("Number of processes in program {} is negative or null", name))
                }
                else {
                    Ok(n as u8)
                }
            }
            None => Err(format!("Wrong format in the number of processes in program {}, expected value must range between 1 and 255", name)),
        }
    }

    fn set_priority(param: &Hash, name: &str) -> Result<u16, String> {
        let priority_yaml = match param.get(&Yaml::String(String::from("priority"))) {
            Some(val) => val,
            None => return Ok(999),
        };
        match priority_yaml.as_i64() {
            Some(n) => {
                if n > 999 {
                    Err(format!(
                        "Priority in program {} is above 999, found {}",
                        name, n
                    ))
                } else if n < 0 {
                    Err(format!("Priority in program {} is negative", name))
                } else {
                    Ok(n as u16)
                }
            }
            None => Err(format!(
                "Wrong format priority in program {}, expected value must range between 0 and 999",
                name
            )),
        }
    }

    fn set_autostart(param: &Hash, name: &str) -> Result<bool, String> {
        let autostart_yaml = match param.get(&Yaml::String(String::from("autostart"))) {
            Some(val) => val,
            None => return Ok(true),
        };
        match autostart_yaml.as_bool() {
            Some(val) => Ok(val),
            None => Err(format!(
                "Bad value for autostart in program {} detected, expecting boolean value",
                name
            )),
        }
    }

    fn set_startsecs(param: &Hash, name: &str) -> Result<u32, String> {
        let startsecs_yaml = match param.get(&Yaml::String(String::from("startsecs"))) {
            Some(val) => val,
            None => return Ok(1),
        };
        match startsecs_yaml.as_i64() {
            Some(n) => {
                if n > 4_294_967_295 {
                    Err(format!("Value provided for number of startsecs in program {} is too high, maximum allowed value is 4 294 967 295, found {}", name, n))
                } else if n < 0 {
                    Err(format!(
                        "Value provided for number of startsecs in program {} is negative",
                        name
                    ))
                } else {
                    Ok(n as u32)
                }
            },
            None =>  Err(format!("Value provided for number of startsecs in program {} not a valid signed 64 bits number", name))
        }
    }

    fn set_startretries(param: &Hash, name: &str) -> Result<u8, String> {
        let startretries_yaml = match param.get(&Yaml::String(String::from("startretries"))) {
            Some(val) => val,
            None => return Ok(3),
        };
        match startretries_yaml.as_i64() {
            Some(n) => {
                if n > 255 {
                    Err(format!("Value provided for number of startretries in program {} is too high, maximum allowed value is 255, found {}", name, n))
                } else if n < 0 {
                    Err(format!(
                        "Value provided for number of startretries in program {} is negative",
                        name
                    ))
                } else {
                    Ok(n as u8)
                }
            }
            None => Err(format!(
                "Value provided for number of startretries in program {} not a valid number",
                name
            )),
        }
    }

    fn set_autorestart(param: &Hash, name: &str) -> Result<RestartState, String> {
        let autorestart_yaml = match param.get(&Yaml::String(String::from("autorestart"))) {
            Some(val) => val,
            None => return Ok(RestartState::ONERROR),
        };
        match autorestart_yaml.as_str() {
            Some(val) => {
                if val == "always" {
                    Ok(RestartState::ALWAYS)
                } else if val == "onerror" {
                    Ok(RestartState::ONERROR)
                } else if val == "never" {
                    Ok(RestartState::NEVER)
                } else {
                    Err(format!("Value provided for autorestart in program {} is invalid, expected values are : \n\t- always\n\t- onerror\n\t- never", name))
                }
            }
            None => Err(format!(
                "Value provided for autorestart in program {} is not a valid string",
                name
            )),
        }
    }

    fn set_exitcodes(param: &Hash, name: &str) -> Result<Vec<u8>, String> {
        let mut codes: Vec<u8> = vec![];
        let exitcodes_vec_yaml = match param.get(&Yaml::String(String::from("exitcodes"))) {
            Some(arr) => arr,
            None => return Ok(vec![0]),
        };
        let exitcodes_vec = match exitcodes_vec_yaml.as_vec() {
            Some(arr) => arr,
            None => {
                return Err(format!(
                    "Value provided for exitcodes in program {} invalid, expected an array",
                    name
                ))
            }
        };
        for code in exitcodes_vec.iter() {
            match code.as_i64() {
                Some(n) => {
                    if n > 255 {
                        return Err(format!("Value provided for exitcodes in program {} is not a valid number, maximum value is 255, found {}", name, n));
                    } else if n < 0 {
                        return Err(format!(
                            "Value provided for exitcodes in program {} is negative",
                            name
                        ));
                    } else {
                        let nu8 = n as u8;
                        if !codes.contains(&nu8) {
                            codes.push(nu8)
                        }
                    }
                }
                None => {
                    return Err(format!(
                        "A value provided for exitcodes in program {} is not a valid number",
                        name
                    ))
                }
            };
        }
        Ok(codes)
    }

    fn set_stopsignal(param: &Hash, name: &str) -> Result<i32, String> {
        let stopsignal_yaml = match param.get(&Yaml::String(String::from("stopsignal"))) {
            Some(val) => val,
            None => return Ok(libc::SIGTERM),
        };
        match stopsignal_yaml.as_str() {
            Some(val) => {
                if val == "HUP" {
                    Ok(libc::SIGHUP)
                } else if val == "USR1" {
                    Ok(libc::SIGUSR1)
                } else if val == "USR2" {
                    Ok(libc::SIGUSR2)
                } else if val == "TERM" {
                    Ok(libc::SIGTERM)
                } else if val == "INT" {
                    Ok(libc::SIGINT)
                } else if val == "QUIT" {
                    Ok(libc::SIGQUIT)
                } else if val == "KILL" {
                    Ok(libc::SIGKILL)
                } else {
                    Err(format!("Value provided for autorestart in program {} is invalid, expected values are : \n\t- TERM\n\t- HUP\n\t- INT\n\t- QUIT\n\t- KILL\n\t- USR1\n\t- USR2", name))
                }
            }
            None => Err(format!(
                "Value provided for autorestart in program {} is not a valid string",
                name
            )),
        }
    }

    fn set_stopwaitsecs(param: &Hash, name: &str) -> Result<u32, String> {
        let stopwaitsecs_yaml = match param.get(&Yaml::String(String::from("stopwaitsecs"))) {
            Some(val) => val,
            None => return Ok(3),
        };
        match stopwaitsecs_yaml.as_i64() {
            Some(n) => {
                if n > 4_294_967_295 {
                    Err(format!("Value provided for number of stopwaitsecs in program {} is too high, maximum allowed value is 4 294 967 295, found {}", name, n))
                } else if n < 0 {
                    Err(format!(
                        "Value provided for number of stopwaitsecs in program {} is negative",
                        name
                    ))
                } else {
                    Ok(n as u32)
                }
            },
            None =>  Err(format!("Value provided for number of stopwaitsecs in program {} not a valid signed 64 bits number", name))
        }
    }

    fn set_stdoutlog(param: &Hash, name: &str) -> Result<bool, String> {
        let stdoutlog_yaml = match param.get(&Yaml::String(String::from("stdoutlog"))) {
            Some(val) => val,
            None => return Ok(true),
        };
        match stdoutlog_yaml.as_bool() {
            Some(val) => Ok(val),
            None => Err(format!(
                "Bad value for stdoutlog in program {} detected, expecting boolean value",
                name
            )),
        }
    }

    fn set_stdoutlogpath(param: &Hash, name: &str) -> Result<Option<String>, String> {
        let stdoutlogpath_yaml = match param.get(&Yaml::String(String::from("stdoutlogpath"))) {
            Some(val) => val,
            None => return Ok(None),
        };
        match stdoutlogpath_yaml.as_str() {
            Some(val) => Ok(Some(String::from(val))),
            None => Err(format!(
                "Bad value for stdoutlogpath in program {} detected, expecting a string",
                name
            )),
        }
    }

    fn set_stderrlog(param: &Hash, name: &str) -> Result<bool, String> {
        let stderrlog_yaml = match param.get(&Yaml::String(String::from("stderrlog"))) {
            Some(val) => val,
            None => return Ok(true),
        };
        match stderrlog_yaml.as_bool() {
            Some(val) => Ok(val),
            None => Err(format!(
                "Bad value for stderrlog in program {} detected, expecting boolean value",
                name
            )),
        }
    }

    fn set_stderrlogpath(param: &Hash, name: &str) -> Result<Option<String>, String> {
        let stderrlogpath_yaml = match param.get(&Yaml::String(String::from("stderrlogpath"))) {
            Some(val) => val,
            None => return Ok(None),
        };
        match stderrlogpath_yaml.as_str() {
            Some(val) => Ok(Some(String::from(val))),
            None => Err(format!(
                "Bad value for stderrlogpath in program {} detected, expecting a string",
                name
            )),
        }
    }

    fn set_env(param: &Hash, name: &str) -> Result<Option<HashMap<String, String>>, String> {
        let env_hash_yaml = match param.get(&Yaml::String(String::from("env"))) {
            Some(arr) => arr,
            None => return Ok(None),
        };
        let env_hash = match env_hash_yaml.as_hash() {
            Some(map) => map,
            None => {
                return Err(format!(
                    "Value provided for env in program {} is invalid, expected a map",
                    name
                ))
            }
        };
        if env_hash.is_empty() {
            return Ok(None);
        }
        let mut new_env: HashMap<String, String> = HashMap::new();
        for (env_key, env_val) in env_hash.iter() {
            let key = match env_key.as_str() {
                Some(k) => String::from(k),
                None => {
                    return Err(format!(
                        "A key provided for env in program {} is not a valid string",
                        name
                    ))
                }
            };
            let val = match env_val.as_str() {
                Some(v) => String::from(v),
                None => {
                    return Err(format!(
                        "Value provided for key {} in the env of program {} is not a valid string",
                        key, name
                    ))
                }
            };
            new_env.insert(key, val);
        }
        Ok(Some(new_env))
    }

    fn set_workingdir(param: &Hash, name: &str) -> Result<Option<String>, String> {
        let workingdir_yaml = match param.get(&Yaml::String(String::from("workingdir"))) {
            Some(val) => val,
            None => return Ok(None),
        };
        match workingdir_yaml.as_str() {
            Some(val) => Ok(Some(String::from(val))),
            None => Err(format!(
                "Bad value for workingdir in program {} detected, expecting a string",
                name
            )),
        }
    }

    fn set_umask(param: &Hash, name: &str) -> Result<Option<u16>, String> {
        let umask_yaml = match param.get(&Yaml::String(String::from("umask"))) {
            Some(val) => val,
            None => return Ok(None),
        };
        match umask_yaml.as_i64() {
            Some(n) => {
                if n > 511 {
                    Err(format!(
                        "Umask in program {} is not a valid octal umask, found {:#o}",
                        name, n
                    ))
                } else if n < 0 {
                    Err(format!("Umask in program {} is negative", name))
                } else {
                    Ok(Some(n as u16))
                }
            }
            None => Err(format!(
                "Wrong format for umask in program {}, expected value must ba a valid octal number between 0o000 and 0o777",
                name
            )),
        }
    }

    fn set_user(param: &Hash, name: &str) -> Result<Option<String>, String> {
        let user_yaml = match param.get(&Yaml::String(String::from("user"))) {
            Some(val) => val,
            None => return Ok(None),
        };
        match user_yaml.as_str() {
            Some(val) => Ok(Some(String::from(val))),
            None => Err(format!(
                "Bad value for user in program {} detected, expecting a string",
                name
            )),
        }
    }
}
