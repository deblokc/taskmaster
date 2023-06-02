use nix::libc;
use yaml_rust::Yaml;

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
    pub env: Option<Vec<(String, String)>>,
    pub workingdir: Option<String>,
    pub umask: Option<u16>,
    pub group: Option<String>,
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
            group: None,
            user: None,
        }
    }

    pub fn set_numprocs(&mut self, numprocs_yaml: &Yaml) -> Option<String> {
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

    pub fn set_priority(&mut self, priority_yaml: &Yaml) -> Option<String> {
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
}
