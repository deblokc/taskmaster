use crate::parsing::program::{Program, RestartState};
use std::{
    collections::HashMap,
    convert::TryFrom,
    io::Error,
    process::{Child, Command, ExitStatus},
    sync::{
        atomic::{AtomicUsize, Ordering},
        Arc, Mutex,
    },
    thread,
    time::{Duration, Instant},
};

#[derive(Debug)]
enum Status {
    STOPPED,  //The process has been stopped or was never started
    STARTING, //The process is starting
    RUNNING,  //The process is running
    BACKOFF,  //The process was STARTING but exited to quickly to move to RUNNING
    STOPPING, //The process is stopping
    EXITED,   //The process exited from RUNNING (expected or unexpected)
    FATAL,    //The process could not be started succesfully
}

#[derive(Debug)]
pub struct Process {
    name: String,                 //name of the process
    pid: u32,                     //pid of the process
    start: Instant,               //process's starting timestamp
    status: Status,               //status of the process (refer to enum Status)
    program: Arc<Mutex<Program>>, //associated program's fully parsed config
    count_restart: u8,            //number of time the program tried to restart
}

impl Process {
    fn new(name: String, program: Arc<Mutex<Program>>) -> Process {
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

    fn start(&mut self) -> Result<Child, Error> {
        let mut command: &mut Command;
        if let Ok(mut program) = self.program.lock() {
            command = &mut Command::new(program.command.clone());
        } else {
            println!("Mutex was poisoned");
            file!();
            line!();
            column!();
        }
        self.status = Status::STARTING;
        self.start = Instant::now();
        loop {
            if let Ok(mut program) = self.program.lock() {
                println!("Starting process with {}", program.command);
                match command.spawn() {
                    Ok(handle) => {
                        return Ok(handle);
                    }
                    Err(er) => {
                        if self.count_restart < program.startretries {
                            self.count_restart += 1;
                            println!("{er}");
                            continue;
                        } else {
                            self.status = Status::FATAL;
                            return Err(er);
                        }
                    }
                }
            } else {
                println!("Mutex was poisoned");
                file!();
                line!();
                column!();
            }
        }
    }

    fn need_restart(&mut self, exit_status: ExitStatus) -> bool {
        if matches!(self.status, Status::STOPPED) || matches!(self.status, Status::FATAL) {
            return false;
        }
        if let Ok(mut program) = self.program.lock() {
            if matches!(self.status, Status::BACKOFF) {
                if self.count_restart < program.startretries {
                    // if can still retry restart
                    self.count_restart += 1;
                    return true;
                } else {
                    // else should go to FATAL
                    self.status = Status::FATAL;
                    return false;
                }
            } else if matches!(program.autorestart, RestartState::NEVER)
                || (matches!(program.autorestart, RestartState::ONERROR)
                    && program.exitcodes.contains(
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
        } else {
            println!("Mutex was poisoned");
            file!();
            line!();
            column!();
            return false;
        }
    }
}

fn change_proc_status(proc_ref: &Arc<Mutex<Process>>) {
    if let Ok(mut proc) = proc_ref.lock() {
        if !matches!(proc.status, Status::STOPPED) && !matches!(proc.status, Status::EXITED)
        // if process wasnt already down
        {
            if matches!(proc.status, Status::STOPPING) {
                // if process was stopped manually
                proc.status = Status::STOPPED;
            }
            if matches!(proc.status, Status::STARTING) {
                proc.status = Status::BACKOFF;
            }
        }
    } else {
        // error on mutex lock
        println!("Mutex was poisoned");
        file!();
        line!();
        column!();
    }
}

fn check_exit_status(
    proc_ref: &Arc<Mutex<Process>>,
    process: &mut Option<Child>,
    exit_status: &mut Option<ExitStatus>,
) {
    if let Some(ref mut process_handle) = process {
        match process_handle.try_wait() {
            // check if process exited
            Ok(Some(status)) => {
                // if it exited
                *exit_status = Some(status);
                change_proc_status(proc_ref);
                *process = None;
            }
            Ok(None) => {} // if it is still running
            Err(e) => {
                // if we couldnt try_wait
                println!("Error with child : {e}");
            }
        }
    }
}

fn check_timed_status(proc_ref: &Arc<Mutex<Process>>, process_handle: &mut Child) {
    if let Ok(mut proc) = proc_ref.lock() {
        if matches!(proc.status, Status::STARTING) {
            if let Ok(program) = proc.program.lock() {
                if proc.start.elapsed() >= Duration::from_secs(program.startsecs.into()) {
                    proc.status = Status::RUNNING;
                }
            }
        } else if matches!(proc.status, Status::STOPPING) {
            /* if start_of_stop > proc.program.stopwaitsecs */
            process_handle.kill();
        }
    } else {
        println!("Mutex was poisoned, exiting administrator");
        return;
    }
}

fn administrator(proc_ref: Arc<Mutex<Process>> /*, mut msg: AtomicUsize*/) {
    let mut process: Option<Child> = None;
    let mut exit_status: Option<ExitStatus> = None;
    loop {
        // check if we have a "message"
        //        if msg.load(Ordering::Relaxed) != 0 {
        /* do shit */
        //            *msg.get_mut() = 0;
        //        }
        if process.is_some() {
            // if we have a process
            check_exit_status(&proc_ref, &mut process, &mut exit_status);
            if let Some(ref mut process_handle) = process {
                check_timed_status(&proc_ref, process_handle);
            }
        } else if exit_status.is_some() {
            // if process ran and exited
            if let Some(status) = exit_status {
                if let Ok(mut proc) = proc_ref.lock() {
                    if proc.need_restart(status) {
                        match proc.start() {
                            Ok(handle) => {
                                process = Some(handle);
                            }
                            Err(e) => {
                                println!("{e}");
                            }
                        }
                    } // if need to restart
                } else {
                    // error on mutex lock
                    println!("Mutex was poisoned");
                    file!();
                    line!();
                    column!();
                }
            }
        } else {
            // if process never ran
            if let Ok(mut proc) = proc_ref.lock() {
                if let Ok(mut program) = proc.program.lock() {
                    if program.autostart && !matches!(proc.status, Status::FATAL) {
                        // if process need to start (autostart = true)
                        match proc.start() {
                            Ok(handle) => {
                                process = Some(handle);
                            }
                            Err(e) => {
                                println!("{e}");
                            }
                        }
                    }
                }
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
    loop {
        for (procname, procmut) in &process {
            println!("{procname}");
            {
                if let Ok(proc) = procmut.lock() {
                    eprintln!("{:?}", proc.status);
                } else {
                    println!("Mutex was poisoned");
                }
            }
        }
        println!();
        thread::sleep(Duration::from_secs(3));
    }
    for child in thread {
        child.join();
    }
}
