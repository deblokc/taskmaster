pub enum RestartState {
    ALWAYS,
    ONERROR,
    NEVER,
}

pub struct Program {
    name: String,
    command: String,
    args: String,
    numprocs: u8,
    priority: u16,
    autostart: bool,
    startsecs: u32,
    startretries: u8,
    autorestart: RestartState,
    exitcodes: Vec<u8>,
    stopsignal: i32,
    stopwaitsecs: u32,
    stdoutlog: bool,
    stdoutlogpath: Option<String>,
    stderrlog: bool,
    stderrlogpath: Option<String>,
    env: Option<Vec<(String, String)>>,
    workingdir: Option<String>,
    umask: Option<u16>,
    group: String,
    user: Option<String>,
}
