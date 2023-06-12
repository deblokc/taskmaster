use crate::parsing::program::RestartState;
use crate::parsing::Program;
use std::collections::HashMap;
use std::convert::TryFrom;
use std::process::{Child, Command, ExitStatus};
use std::thread;
use std::time::{Duration, Instant};

enum Status {
    STOPPED,  //The process has been stopped or was never started
    STARTING, //The process is starting
    RUNNING,  //The process is running
    BACKOFF,  //The process was STARTING but exited to quickly to move to RUNNING
    STOPPING, //The process is stopping
    EXITED,   //The process exited from RUNNING (expected or unexpected)
    FATAL,    //The process could not be started succesfully
}

pub struct Process<'a> {
    name: String,         //name of the process
    pid: u32,             //pid of the process
    start: Instant,       //process's starting timestamp
    status: Status,       //status of the process (refer to enum Status)
    program: &'a Program, //associated program's fully parsed config
    count_restart: u8,    //number of time the program tried to restart
}

impl Process<'_> {
    fn new(name: String, program: &Program) -> Process {
        let pid = 0;
        let start = Instant::now();
        let status = Status::STARTING;
        let count_restart: u8 = 0;

        Process {
            name,
            pid,
            start,
            status,
            program,
            count_restart,
        }
    }

    fn start(&mut self, cmd: &str) -> Child {
        let command = &mut Command::new(cmd);

        self.status = Status::STARTING;
        let child = command.spawn().expect("Faut wrapper tout ca");
        child
    }

    fn need_restart(&mut self, exit_status: ExitStatus) -> bool {
        if matches!(self.status, Status::STOPPED) {
            return false;
        }
        if matches!(self.status, Status::BACKOFF) {
            // if failed while starting
            if self.count_restart < self.program.startretries {
                // if can still retry restart
                self.count_restart += 1;
                return true;
            } else {
                // if done trying goes to FATAL
                self.status = Status::FATAL;
                return false;
            }
        } else if matches!(self.program.autorestart, RestartState::NEVER)
            || (matches!(self.program.autorestart, RestartState::ONERROR)
                && self.program.exitcodes.contains(
                    &(u8::try_from(exit_status.code().expect("no exit status"))
                        .ok()
                        .expect("could not convert")),
                ))
        {
            // if no autorestart or expected exit dont restart
            self.status = Status::EXITED;
            return false;
        } else {
            // else (autorestart always or unexpected exit) restart
            self.status = Status::EXITED;
            return true;
        }
    }
}

fn administrator(mut proc: Process) {
    let mut process_handle = proc.start(&proc.program.command);
    proc.start = Instant::now();

    loop {
        match process_handle.try_wait() {
            Ok(Some(exit_status)) => {
                if !matches!(proc.status, Status::STOPPED) && !matches!(proc.status, Status::EXITED)
                {
                    if matches!(proc.status, Status::STOPPING) {
                        // if process was stopped manually
                        proc.status = Status::STOPPED;
                    }
                    if matches!(proc.status, Status::STARTING) {
                        // if process could not start
                        proc.status = Status::BACKOFF;
                    }
                    if proc.need_restart(exit_status) {
                        let mut process_handle = proc.start(&proc.program.command);
                        //restart the process
                    }
                }
            }
            Ok(None) => {
                continue;
            }
            Err(e) => {
                println!("Error with child : {e}");
            }
        }
        if matches!(proc.status, Status::STARTING) {
            if proc.start.elapsed() >= Duration::from_secs(proc.program.startsecs.into()) {
                proc.status = Status::RUNNING;
            }
        } else if matches!(proc.status, Status::STOPPING) {
            /* if start_of_stop > proc.program.stopwaitsecs */
            process_handle.kill();
        }
    }
}

pub fn infinity() {
    let mut programs: HashMap<i16, Vec<Program>> = HashMap::new();
    let mut priorities: Vec<i16> = Vec::new();
    let mut process: HashMap<String, Process> = HashMap::new();
    let mut thread: Vec<thread::JoinHandle<()>> = Vec::new();

    loop {
        //(programs, priorities) = parsing(file);
        priorities.sort();
        for key in &priorities {
            if key > &999 {
                return;
            }
            let tmp = match programs.get(key) {
                // Get the vector of programs associated with this priority
                Some(x) => x,
                _ => todo!(),
            };
            for proc in tmp {
                if proc.numprocs == 1 {
                    let current = Process::new(proc.name.clone(), proc);
                    process.insert(current.name.clone(), current);
                    // if only one process use the name given
                    thread.push(thread::spawn(move || administrator(current)));
                } else {
                    for i in 0..proc.numprocs {
                        let current = Process::new(proc.name.clone() + &i.to_string(), proc);
                        process.insert(current.name.clone(), current);
                        // if multiple process add number after
                    }
                }
                // Create process and insert them in the map with name as key
            }
        }
    }
}
