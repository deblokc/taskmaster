use std::collections::HashMap;
use std::process::{Child, Command, ExitStatus};
use std::thread;
use std::time::{Instant, Duration};

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
    name: String,    //name of the process
    pid: u32,        //pid of the process
    start: Instant,  //process's starting timestamp
    status: Status,  //status of the process (refer to enum Status)
    program: String, //associated program's fully parsed config
}

impl Process {
    fn new(name: String, program: String) -> Process {
        let pid = 0;
        let start = Instant::now();
        let status = Status::STARTING;

        Process {
            name,
            pid,
            start,
            status,
            program,
        }
    }

    fn start(&self, cmd: &str) -> Child {
        let command = &mut Command::new(cmd);

        let child = command.spawn().expect("Faut wrapper tout ca");
        child
    }

    fn need_restart(&self, exit_status: ExitStatus) -> bool {
        if self.status == Status::STARTING {// if program wasnt running
            self.status = Status::BACKOFF;
            return true;
        } else if ExitStatus in self.program.exit_status { // if exited properly
            return false;
        } else {
            return true;
        }
    }
}

fn administrator(proc: Process) {
    let mut process_handle = proc.start("ls");
    proc.start = Instant::now();
    proc.status = Status::STARTING;

    process_handle.spawn();
    loop {
        match process_handle.try_wait() {
            Ok(Some(exit_status)) => {
                if !proc.need_restart(exit_status) {
                    proc.status = Status::STOPPED;
                    break;
                } else {
                    process_handle.spawn();
                    //restart the process
                }
            }// if no restart exit, else try restart;
            Ok(None) => continue;
            Err(e) => {
                println!("Error with child : {e}");
                break;
            }
        }
        if proc.status == Status::STARTING {
        }
    }
}

pub fn infinity() {
    let mut programs: HashMap<i16, Vec<String>> = HashMap::new();
    let mut priorities: Vec<i16> = Vec::new();
    let mut process: HashMap<String, Process> = HashMap::new();
    let mut thread: Vec<thread::JoinHandle<_>> = Vec::new();

    loop {
        //(programs, priorities) = parsing(file);
        priorities.sort();
        for key in &priorities {
            let tmp = match programs.get(key) {
                // Get the vector of programs associated with this priority
                Some(x) => x,
                _ => todo!(),
            };
            for proc in tmp {
                if proc.numprocs == 1 {
                    let current = Process::new(proc.name, proc);
                    process.insert(current.name, current);
                    // if only one process use the name given
                    thread.push(thread::spawn(move || {administrator(current)}));
                } else {
                    for i in 0..proc.numprocs {
                        let current = Process::new(proc.name + i, proc);
                        process.insert(current.name, current);
                        // if multiple process add number after
                    }
                }
                // Create process and insert them in the map with name as key
            }
        }
    }
}
