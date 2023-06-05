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
    count_restart: u64,
}

impl Process {
    fn new(name: String, program: String) -> Process {
        let pid = 0;
        let start = Instant::now();
        let status = Status::STARTING;
        let count_restart = 0;

        Process {
            name,
            pid,
            start,
            status,
            program,
            count_restart,
        }
    }

    fn start(&self, cmd: &str) -> Child {
        let command = &mut Command::new(cmd);

        self.status = Status::STARTING;
        let child = command.spawn().expect("Faut wrapper tout ca");
        child
    }

    fn need_restart(&self, exit_status: ExitStatus) -> bool {
        if self.status == Status::STOPPED {
            return false;
        }
        if self.status == Status::BACKOFF { // if failed while starting
            if self.count_restart < self.program.startretries { // if can still retry restart
                self.count_restart += 1;
                return true;
            } else { // if done trying goes to FATAL
                self.status = Status::FATAL;
                return false;
            }
        } else if (self.program.autorestart == Program::NEVER || (self.program.autorestart == Program::UNEXPECTED && ExitStatus.code() in self.program.exitcodes) {
            // if no autorestart or expected exit dont restart
            self.status = Status::EXITED;
            return false;
        } else { // else (autorestart always or unexpected exit) restart
            self.status = Status::EXITED;
            return true;
        }
    }
}

fn administrator(proc: Process) {
    let mut process_handle = proc.start(proc.program.command);
    proc.start = Instant::now();

    loop {
        match process_handle.try_wait() {
            Ok(Some(exit_status)) => {
                if proc.status != Status::STOPPED && proc.status != Status::EXITED {
                    if proc.status == Status::STOPPING {
                        // if process was stopped manually
                        proc.status = Status::STOPPED;
                    }
                    if proc.status == Status::STARTING {
                        // if process could not start 
                        proc.status = Status::BACKOFF;
                    }
                    if proc.need_restart(exit_status) {
                        let mut process_handle = proc.start(proc.program.command);
                        //restart the process
                    }
                }
            }
            Ok(None) => continue;
            Err(e) => {
                println!("Error with child : {e}");
            }
        }
        if proc.status == Status::STARTING {
            if proc.start.elapsed() >= Duration::from_secs(proc.program.startsecs) {
                proc.status = Status::RUNNING;
            }
        } else if proc.status == Status::STOPPING {
            /* if start_of_stop > proc.program.stopwaitsecs */
            proc.kill();
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
