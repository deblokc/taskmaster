use std::process::{Child, Command};
use std::time::Instant;

enum Status {
    STOPPED,  //The process has been stopped or was never started
    STARTING, //The process is starting
    RUNNING,  //The process is running
    BACKOFF,  //The process was STARTING but exited to quickly to move to RUNNING
    STOPPING, //The process is stopping
    EXITED,   //The process exited from RUNNING (expected or unexpected)
    FATAl,    //The process could not be started succesfully
}

pub struct Process<'a> {
    name: String,      //name of the process
    cmd: &'a str,      //command to run
    pid: u32,          //pid of the process
    args: Vec<String>, //arguments of the command
    umask: String,     //umask of the process
    start: Instant,    //process's starting timestamp
    status: Status,    //status of the process (refer to enum Status)
    startretries: u32, //number of restart try before FATAL
    startsecs: u32,    //time after which the process is considered running
    priority: u32,
}

impl Process<'_> {
    fn new(name: String, cmd: &str) -> Process {
        let pid = 0;
        let args: Vec<String> = Vec::new();
        let umask = String::from("022");
        let start = Instant::now();
        let status = Status::STARTING;
        let startretries = 3;
        let startsecs = 1;
        let priority = 999;

        Process {
            name,
            cmd,
            pid,
            args,
            umask,
            start,
            status,
            startretries,
            startsecs,
            priority,
        }
    }

    fn start(&self) -> Child {
        let command = &mut Command::new(self.cmd);
        let child = command.spawn().expect("Faut wrapper tout ca");
        child
    }
}
