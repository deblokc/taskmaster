use crate::parsing::program::Program;
use crate::parsing::program::RestartState;
use std::collections::HashMap;
use std::convert::TryFrom;
use std::process::{Child, Command, ExitStatus};
use std::sync::{Arc, Mutex};
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

pub struct Process {
    name: String,          //name of the process
    pid: u32,              //pid of the process
    start: Instant,        //process's starting timestamp
    status: Status,        //status of the process (refer to enum Status)
    program: Arc<Program>, //associated program's fully parsed config
    count_restart: u8,     //number of time the program tried to restart
}

impl Process {
    fn new(name: String, program: Arc<Program>) -> Process {
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

    fn start(&mut self) -> Child {
        let command = &mut Command::new(self.program.command.clone());

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

fn administrator(proc_ref: Arc<Mutex<Process>>) {
    if let Ok(mut proc) = proc_ref.lock() {
        let mut process_handle = proc.start();
        proc.start = Instant::now();

        loop {
            match process_handle.try_wait() {
                Ok(Some(exit_status)) => {
                    if !matches!(proc.status, Status::STOPPED)
                        && !matches!(proc.status, Status::EXITED)
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
                            process_handle = proc.start();
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
}

pub fn infinity(programs: &HashMap<u16, Vec<Arc<Program>>>) {
    //    let mut programs: HashMap<u16, Vec<&Program>> = HashMap::new();
    let mut process: HashMap<String, Arc<Mutex<Process>>> = HashMap::new();
    let mut thread: Vec<thread::JoinHandle<()>> = Vec::new();

    //  loop {
    //(programs, priorities) = parsing(file);
    let mut priorities: Vec<&u16> = programs.keys().collect();
    priorities.sort();
    for key in priorities {
        let tmp = programs.get(key).expect("no value for this key");
        for prog in tmp {
            if prog.numprocs == 1 {
                let current = Arc::new(Mutex::new(Process::new(
                    prog.name.clone(),
                    Arc::clone(prog),
                ))); //Mutex::new(Process::new(prog.name.clone(), prog)));
                process.insert(prog.name.clone(), Arc::clone(&current));
                // if only one process use the name given
                thread.push(thread::spawn(move || administrator(current)));
                // Arc<Mutex<Process>>
            } else {
                for i in 0..prog.numprocs {
                    let current = Arc::new(Mutex::new(Process::new(
                        prog.name.clone() + &i.to_string(),
                        Arc::clone(prog),
                    )));
                    process.insert(prog.name.clone() + &i.to_string(), Arc::clone(&current));
                    // if multiple process add number after
                    thread.push(thread::spawn(move || administrator(current)));
                }
            }
            // Create process and insert them in the map with name as key
        }
    }
    // }
    for child in thread {
        child.join();
    }
}
