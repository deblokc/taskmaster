use crate::parsing::program::{Program, RestartState};
use std::{
    collections::HashMap,
    convert::TryFrom,
    fmt,
    io::Error,
    process::{Child, Command, ExitStatus},
    sync::{
        atomic::{AtomicUsize, Ordering},
        Arc, Mutex, MutexGuard,
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

enum Log {
    CRIT, //Fatal error of taskmaster (Panic, expect, mutex poisoning)
    ERRO, //Unexepected issue (process going FATAL, cannot write log, ...)
    INFO, //Basic information about process (spawn of process, status change, ...)
    DEBG, //Print for debug of taskmaster itself only
}

impl fmt::Display for Log {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Log::CRIT => write!(f, "CRIT"),
            Log::ERRO => write!(f, "ERRO"),
            Log::INFO => write!(f, "INFO"),
            Log::DEBG => write!(f, "DEBG"),
        }
    }
}

fn log(level: Log, msg: String) {
    // need to add a global to print only expected log
    println!("{} {}", level, msg);
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

    fn start(&mut self, program: &MutexGuard<'_, Program>) -> Result<Child, Error> {
        let mut command = Command::new(program.command.clone());

        if let Some(arguments) = &program.args {
            command.args(arguments);
        }

        self.status = Status::STARTING;
        self.start = Instant::now();
        loop {
            match command.spawn() {
                Ok(handle) => {
                    self.pid = handle.id();
                    log(
                        Log::INFO,
                        format!("spawned: {} with pid {}", self.name, self.pid),
                    );
                    return Ok(handle);
                }
                Err(er) => {
                    if self.count_restart < program.startretries {
                        self.count_restart += 1;
                        log(Log::INFO, format!("restart: {} (cause : {er})", self.name));
                        continue;
                    } else {
                        self.status = Status::FATAL;
                        log(Log::ERRO, format!("status: {} FATAL", self.name));
                        return Err(er);
                    }
                }
            }
        }
    }

    fn need_restart(&mut self, exit_status: ExitStatus) -> bool {
        if matches!(self.status, Status::STOPPED)
            || matches!(self.status, Status::FATAL)
            || matches!(self.status, Status::EXITED)
        {
            return false;
        }
        log(
            Log::DEBG,
            format!(
                "{} in need_restart with status {:?}",
                self.name, self.status
            ),
        );
        log(
            Log::DEBG,
            format!(
                "{} is trying to lock its program at {} {}",
                self.name,
                file!(),
                line!()
            ),
        );
        if let Ok(program) = self.program.lock() {
            log(
                Log::DEBG,
                format!(
                    "{} has locked its program {} {}",
                    self.name,
                    file!(),
                    line!()
                ),
            );
            if matches!(self.status, Status::BACKOFF) {
                if self.count_restart < program.startretries {
                    // if can still retry restart
                    self.count_restart += 1;
                    log(
                        Log::INFO,
                        format!("restart: {} (cause : BACKOFF)", self.name),
                    );
                    return true;
                } else {
                    // else should go to FATAL
                    self.status = Status::FATAL;
                    log(Log::ERRO, format!("status: {} FATAL", self.name));
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
                log(Log::INFO, format!("status: {} EXITED", self.name));
                return false;
            } else {
                // else (autorestart always or unexpected exit) restart
                self.status = Status::EXITED;
                log(Log::INFO, format!("status: {} EXITED", self.name));
                if matches!(program.autorestart, RestartState::ALWAYS) {
                    log(
                        Log::INFO,
                        format!("restart: {} (cause : autorestart ALWAYS)", self.name),
                    );
                } else {
                    log(
                        Log::INFO,
                        format!("restart: {} (cause : unexpected exit status)", self.name),
                    );
                }
                return true;
            }
        } else {
            log(Log::CRIT, format!("mutex: Mutex was poisoned"));
            file!();
            line!();
            column!();
            return false;
        }
    }
}

fn change_proc_status(proc_ref: &Arc<Mutex<Process>>) {
    //log(Log::DEBG, format!("{} is trying to lock its process", name));
    if let Ok(mut proc) = proc_ref.lock() {
        log(
            Log::DEBG,
            format!(
                "{} has locked its process {} {}",
                proc.name,
                file!(),
                line!()
            ),
        );
        if !matches!(proc.status, Status::STOPPED) && !matches!(proc.status, Status::EXITED)
        // if process wasnt already down
        {
            if matches!(proc.status, Status::STOPPING) {
                // if process was stopped manually
                proc.status = Status::STOPPED;
                log(Log::INFO, format!("status: {} STOPPED", proc.name));
            }
            if matches!(proc.status, Status::STARTING) {
                // if process was starting
                proc.status = Status::BACKOFF;
                log(Log::INFO, format!("status: {} BACKOFF", proc.name));
            }
        }
        log(
            Log::DEBG,
            format!(
                "{} has unlocked its process {} {}",
                proc.name,
                file!(),
                line!()
            ),
        );
    } else {
        // error on mutex lock
        log(Log::CRIT, format!("mutex: Mutex was poisoned"));
        file!();
        line!();
        column!();
    }
}

fn check_exit_status(
    proc_ref: &Arc<Mutex<Process>>,
    process: &mut Option<Child>,
    exit_status: &mut Option<ExitStatus>,
    name: &String,
) {
    if let Some(ref mut process_handle) = process {
        match process_handle.try_wait() {
            // check if process exited
            Ok(Some(status)) => {
                // if it exited
                *exit_status = Some(status);
                log(Log::INFO, format!("exited: {name} ({status})"));
                change_proc_status(proc_ref);
                *process = None;
            }
            Ok(None) => {} // if it is still running
            Err(e) => {
                // if we couldnt try_wait
                log(Log::ERRO, format!("try_wait failed : {e}"));
            }
        }
    }
}

fn check_timed_status(proc_ref: &Arc<Mutex<Process>>, process_handle: &mut Child) {
    //log(Log::DEBG, format!("{} is trying to lock its process", name));
    if let Ok(mut proc) = proc_ref.lock() {
        // log(Log::DEBG,format!("{} has locked its process {} {}",proc.name,file!(),line!()),);
        if matches!(proc.status, Status::STARTING) {
            let cloned = proc.program.clone();
            // log(Log::DEBG,format!("{} is trying to lock its program", proc.name),);
            if let Ok(program) = cloned.lock() {
                // log(Log::DEBG,format!("{} has locked its program {} {}",proc.name,file!(),line!()),);
                if proc.start.elapsed() >= Duration::from_secs(program.startsecs as u64) {
                    proc.status = Status::RUNNING;
                    log(Log::INFO, format!("status: {} RUNNING", proc.name));
                }
                // log(Log::DEBG,format!("{} has unlocked its program {} {}",proc.name,file!(),line!()),);
            };
        } else if matches!(proc.status, Status::STOPPING) {
            /* if start_of_stop > proc.program.stopwaitsecs */
            process_handle.kill();
        }
    // log(Log::DEBG,format!("{} has unlocked its process {} {}",proc.name,file!(),line!()),);
    } else {
        // error on mutex lock
        log(Log::CRIT, format!("mutex: Mutex was poisoned"));
    }
}

fn check_restart(proc_ref: &Arc<Mutex<Process>>, status: &ExitStatus, process: &mut Option<Child>) {
    // log(Log::DEBG, format!("{} is trying to lock its process", name));
    if let Ok(mut proc) = proc_ref.lock() {
        // log(Log::DEBG,format!("{} has locked its process {} {}",proc.name,file!(),line!()),);
        if proc.need_restart(*status) {
            log(
                Log::DEBG,
                format!(
                    "{} has unlocked its program {} {}",
                    proc.name,
                    file!(),
                    line!()
                ),
            );
            // if restart needed
            let cloned = proc.program.clone();
            if let Ok(program) = cloned.lock() {
                match proc.start(&program) {
                    Ok(handle) => {
                        *process = Some(handle);
                    }
                    Err(e) => {
                        println!("{e}");
                    }
                }
            };
        } else {
            // log(Log::DEBG,format!("{} has unlocked its program {} {}",proc.name,file!(),line!()),);
        } // if need to restart
          // log(Log::DEBG,format!("{} has unlocked its process {} {}",proc.name,file!(),line!()),);
        drop(proc);
    } else {
        // error on mutex lock
        log(Log::CRIT, format!("mutex: Mutex was poisoned"));
        file!();
        line!();
        column!();
    }
}

fn first_start(proc_ref: &Arc<Mutex<Process>>, process: &mut Option<Child>) {
    //log(Log::DEBG, format!("{} is trying to lock its process", name));
    if let Ok(mut proc) = proc_ref.lock() {
        log(
            Log::DEBG,
            format!(
                "{} has locked its process {} {}",
                proc.name,
                file!(),
                line!()
            ),
        );
        let cloned = proc.program.clone();
        log(
            Log::DEBG,
            format!("{} is trying to locke its program", proc.name),
        );
        if let Ok(program) = cloned.lock() {
            log(
                Log::DEBG,
                format!(
                    "{} has locked its program {} {}",
                    proc.name,
                    file!(),
                    line!()
                ),
            );
            if program.autostart && !matches!(proc.status, Status::FATAL) {
                // if process need to start (autostart = true)
                match proc.start(&program) {
                    Ok(handle) => {
                        *process = Some(handle);
                    }
                    Err(e) => {
                        log(Log::ERRO, format!("start failed : {e}"));
                    }
                }
            }
            log(
                Log::DEBG,
                format!(
                    "{} has unlocked its program {} {}",
                    proc.name,
                    file!(),
                    line!()
                ),
            );
        };
        log(
            Log::DEBG,
            format!(
                "{} has unlocked its process {} {}",
                proc.name,
                file!(),
                line!()
            ),
        );
    }
}

fn administrator(proc_ref: Arc<Mutex<Process>> /*, mut msg: AtomicUsize*/) {
    let name: String;
    {
        let proc = proc_ref.lock().expect("could not fail ?");
        name = proc.name.clone();
        drop(proc);
    }
    log(Log::INFO, format!("start: administrator of {name} started"));
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
            check_exit_status(&proc_ref, &mut process, &mut exit_status, &name);
            if let Some(ref mut process_handle) = process {
                check_timed_status(&proc_ref, process_handle);
            }
        } else if exit_status.is_some() {
            // if process ran and exited
            if let Some(status) = exit_status {
                check_restart(&proc_ref, &status, &mut process);
            }
        } else {
            // if process never ran
            first_start(&proc_ref, &mut process);
        }
    }
}

pub fn infinity(programs: &HashMap<u16, Vec<Arc<Mutex<Program>>>>) {
    //    let mut programs: HashMap<u16, Vec<&Program>> = HashMap::new();
    let mut process: HashMap<String, Arc<Mutex<Process>>> = HashMap::new();
    let mut thread: Vec<thread::JoinHandle<()>> = Vec::new();

    //  loop {
    //(programs, priorities) = parsing(file);
    let mut priorities: Vec<&u16> = programs.keys().collect();
    priorities.sort();

    println!("\n =-=-=-=-=-=-=-= CREATING EVERY ADMINISTRATOR AND PROCESS =-=-=-=-=-=-=-= \n");

    for key in priorities {
        let tmp = programs.get(key).expect("no value for this key");
        for prog in tmp {
            println!("\n\nprog : {:?}", prog);
            if let Ok(program) = prog.lock() {
                if program.numprocs == 1 {
                    let current = Arc::new(Mutex::new(Process::new(
                        program.name.clone(),
                        Arc::clone(prog),
                    ))); //Mutex::new(Process::new(prog.name.clone(), prog)));
                    process.insert(program.name.clone(), Arc::clone(&current));
                    // if only one process use the name given
                    thread.push(thread::spawn(move || administrator(current)));
                    // Arc<Mutex<Process>>
                } else {
                    for i in 0..program.numprocs {
                        let current = Arc::new(Mutex::new(Process::new(
                            program.name.clone() + &i.to_string(),
                            Arc::clone(prog),
                        )));
                        process.insert(program.name.clone() + &i.to_string(), Arc::clone(&current));
                        // if multiple process add number after
                        thread.push(thread::spawn(move || administrator(current)));
                    }
                }
                // Create process and insert them in the map with name as key
            }
        }
    }
    println!("\n =-=-=-=-=-=-=-= CREATED EVERY ADMINISTRATOR NOW MONITORING =-=-=-=-=-=-=-=-= \n");
    // }
    loop {
        for (procname, procmut) in &process {
            {
                if let Ok(proc) = procmut.lock() {
                    println!("{procname} : {:?}", proc.status);
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
