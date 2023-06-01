use std::process::{Child, Command};
use std::time::Instant;

enum Status {
    STOPPED,  //The process has been stopped or was never started
    STARTING, //The process is starting
    RUNNING,  //The process is running
    BACKOFF,  //The process was STARTING but exited to quickly to move to RUNNING
    STOPPING, //The process is stopping
    EXITED,   //The process exited from RUNNING (expected or unexpected)
    FATAL,    //The process could not be started succesfully
}

pub struct Process {
    name: String,   //name of the process
    pid: u32,       //pid of the process
    start: Instant, //process's starting timestamp
    status: Status, //status of the process (refer to enum Status)
}

impl Process {
    fn new(name: String) -> Process {
        let pid = 0;
        let start = Instant::now();
        let status = Status::STARTING;

        Process {
            name,
            pid,
            start,
            status,
        }
    }

    fn start(&self, cmd: &str) -> Child {
        let command = &mut Command::new(cmd);
        let child = command.spawn().expect("Faut wrapper tout ca");
        child
    }
}
