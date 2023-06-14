use std::fmt;

#[derive(Debug)]
pub enum Log {
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

impl Log {
    pub fn log(level: Log, msg: String) {
        // need to add a global to print only expected log
        println!("{} {}", level, msg);
    }
}
