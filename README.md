# Taskmaster
Taskmaster is a multi-threaded client/server system that allows its users to monitor and control a number of processes on UNIX-like operating systems, similar to [supervisor](http://supervisord.org/)
Unlike some processes managers it is **not** meant to be run as substitute for `init` as PID 1. It is meant to be a control tool for an UNIX environment and is expected to start like any other program.
## Taskmaster executables
#### taskmasterd

The server part is taskmasterd that can be daemonized. It is configured by a file in [YAML](https://yaml.org/) and creates a thread for each process it need to run. It handle connection on UNIX socket from controllers, and log during function event categorized in this list : `DEBUG, INFO, WARNING, ERROR, CRITICAL`.
Each thread monitor its own process, log `stdout` and `stderr` output, restart it if it crashed or stopped according to the configuration and perform action if a controller requested it.

#### taskmasterctl

The command-line interface of taskmaster is taskmasterctl. It has shell like features, and allow you to get information on running processes, change configuration without needing to stop the server, check logs or attach to a running process.
The client connect to the server using UNIX socket and can require an authentication if specified in the configuration file.

## Installing

To install the project, you need to first clone the project.
```git clone https://github.com/deblokc/taskmaster.git```

Then you can run.
```make```
(or if you want supervisord to log on discord).
```make DISCORD=yes```
To switch between these option if the executable was already build, add `re`
like :
```make re``` or ```make re DISCORD=yes```

## Running taskmaster

#### taskmasterd
taskmasterd take a configuration file as argument.
`./taskmasterd config_files/config.yml`

For the socket authentication, it can also generate a hash to be put in the configuration file.
`./taskmasterd -x <Password>`
`<Hash>`

Once supervisord is running, it can be stopped by `ctrl + c` if it is in foreground.

To change its configuration, modify the file and send a `SIGHUP` to the running process.

#### taskmasterctl
taskmasterctl take a configuration file as argument to get the path of the socket.
`./taskmasterctl config_files/config.yml`

It can be run without, and it will try to connect at `/tmp/taskmaster.sock`.

There are a few command available in taskmasterctl : 
```
exit        Exit the taskmaster shell.

maintail -f     Continuous tail of taskmaster main log file (Ctrl-D to exit)
maintail -100   last 100 *bytes* of taskmaster main log file
maintail        last 1600 *bytes* of taskmaster main log file

quit    Exit the taskmaster shell.

signal <signal name> <name>           Signal a process
signal <signal name> <name> <name>   Signal multiple processes or groups
signal <signal name> all               Signal all processes

stop <name>            Stop a process
stop <name> <name>    Stop multiple processes or groups
stop all                Stop all processes

fg <process>   Connect to a process in foreground mode
       Ctrl-D to exit

reload          Restart the remote taskmasterd.

restart <name>             Restart a process
restart <name> <name>     Restart multiple processes or groups
restart all                 Restart all processes

start <name>               Start a process
start <name> <name>       Start multiple processes or groups
start all                   Start all processes

tail [-f] <name> [stdoutstderr]   (default stdout)
x:
tail -f <name>         Continuous tail of named process stdout
        Ctrl-D to exit
tail -100 <name>       last 100 *bytes* of process stdout
tail <name> stderr     last 1600 *bytes* of process stderr

help                Print a list of available commands
help <command>     Print help for <command>

pid             Get the PID of taskmasterd.
pid <name>     Get the PID of a single child process by name.
pid all         Get the PID of every child process, one per line.

shutdown        Shut the remote taskmasterd down.

status <name>          Get status for a single process
status <name> <name>  Get status for multiple named processes
status                  Get all process status info

update          Reload config and add/remove as necessary, and will restart affected programs
update all      Reload config and add/remove as necessary, and will restart affected programs
```

## Configuration files

The configuration file is in YAML, and has 3 main fields (all optional). Any unknown field will be ignored. Keep in mind that the Discord field requires that you've build the executables using `make DISCORD=true`, else it will simply ignore it.

#### programs:
```yaml
programs:                                       # Block of programs which will be managed by taskmasterd. If a program name is defined twice or more, only its last definition will remain
    programname:                                # [MANDATORY] Name of the program
        command: /bin/sh                        # [MANDATORY] Command to run. Note that it does not support environement variable expansions and escape sequences are limited to double quotes and single quotes according to bash POSIX behavior.
        numprocs: 1                             # Number of processes to run for this program. Note that a hard limit of 100 total processes is imposed. Accepted values range from 1 to 100. Default value: 1
        priority: 50                            # Priority is used to determine the order in which programs will be started and stopped. Lower starts first and terminates last. Accepted values range from 0 to 999. Default value: 999
        autostart: true                         # Determines whether the program should be started when taskmasterd starts. If false the program can be started via taskmasterctl. Accepted values are true, True, on, yes, false, False, off, no. Default value: true
        startsecs: 10                           # Number of seconds to wait until the program is considered as successfully started (when it moves from STARTING to RUNNING state). Accepted values range from 0 to 300. Default value: 3
        startretries: 3                         # Number of times taskmasterd should try to start the program if it fails to move from STARTING to RUNNING state. Accepted values range from 1 to 10. Default value: 3
        autorestart: onerror                    # Determines when taskmasterd should automatically try to restart a program in RUNNING state which exits. Accepted values are never, onerror, always. Default value: onerror
        exitcodes:                              # List of exit codes which are considered normal exit codes (i.e.: No error happened). This value is used for the autorestart property if it is set as onerror. Accepted values range from 0 to INT_MAX. Default value: 0 
            - 0                                  
            - 1
        stopsignal: TERM                        # Signal to be sent to the process to stop it. Accepted values are TERM, HUP, INT, QUIT, KILL, USR1, USR2. Default value: TERM
        stopwaitsecs: 10                        # Number of seconds taskmasterd should leave the process to terminate when it sends a stopsignal before forcingly killing it. Accepted values range from 0 to 300. Default value: 5
        stdoutlog: True                         # Whether taskmasterd should log the stdout of the process. Accepted values are true, True, on, yes, false, False, off, no. Default value: true
        stdout_logfile: /tmp/programname-out    # Path to the stdout logfile for the process if logging is enabled.
        stdout_logfile_maxbytes: 100            # Maximum number of bytes for each stdout logfile. Accepted values range from 100 to 1073741824 (1Go). Default value: 5242880 (5Mo)
        stdout_logfile_backups: 30              # Number of backups to keep for the stdout logfiles. Numbers will be added at the end of the chosen filename starting from 1 to the number of backups chosen. Accepted values range from 0 to 100. Default value: 10
        stderrlog: True                         # Whether taskmasterd should log the stderr of the process. Accepted values are true, True, on, yes, false, False, off, no. Default value: true
        stderr_logfile: /tmp/programname-out    # Path to the stderr logfile for the process if logging is enabled.
        stderr_logfile_maxbytes: 100            # Maximum number of bytes for each stderr logfile. Accepted values range from 100 to 1073741824 (1Go). Default value: 5242880 (5Mo)
        stderr_logfile_backups: 30              # Number of backups to keep for the stderr logfiles. Numbers will be added at the end of the chosen filename starting from 1 to the number of backups chosen. Accepted values range from 0 to 100. Default value: 10
        workingdir: /home/username              # Working directory for the process.
        umask: 022                              # Umask to be applied to the process in octal. Accepted values range from 0 to 0777 (in octal). Default value: 022 (in octal)
        env:                                    # Environment variables for the process.
            username: john
        user: johnny                            # User which should be running the process (user will be set with setuid)
```

#### server:
```yaml
server:                                         # Taskmasterd general server configuration
    logfile: /tmp/taskmasterd.log               # Path to taskmasterd logfile
    logfile_maxbytes: 100                       # Maximum number of bytes for each stdout logfile. Accepted values range from 100 to 1073741824 (1Go). Default value: 5242880 (5Mo)
    logfile_backups: 30                         # Number of backups to keep for the stdout logfiles. Numbers will be added at the end of the chosen filename starting from 1 to the number of backups chosen. Accepted values range from 0 to 100. Default value: 10
    log_discord: true                           # Whether logging to Discord should be enabled. Accepted values are true, True, on, yes, false, False, off, no. Default value: false
    loglevel_discord: WARN                      # The log level which should be applied for Discord. Accepted values are DEBUG, INFO, WARN, ERROR, CRITICAL. It is not recommended to enable DEBUG logging to Discord for performance reasons.
    discord_token: <TOKEN>                      # The Discord token to use the API. The token can also be exported as en environment variable named DISCORD_TOKEN before starting taskmasterd, which improves privacy. Mandatory if logging to Discord is enabled.
    discord_channel: <CHANNEL>                  # The Discord channel to which the messages should be sent. The channel can also be exported as en environment variable named DISCORD_CHANNEL before starting taskmasterd, which improves privacy. Mandatory if logging to Discord is enabled.
    pidfile: /tmp/.taskmasterd.pid              # The path to the PID file for taskmasterd
    user: john                                  # User which should be running taskmasterd (user will be set with setuid)
    workingdir: /home/username                  # Working directory for taskmasterd.
    umask: 022                                  # Umask to be applied to the process in octal. Accepted values range from 0 to 0777 (in octal). Default value: 022 (in octal)
    daemon: false                               # Whether taskmasterd should be started as a daemon or not. If taskmasterd is not running as a daemon, logs will be displayed in the shell according to the loglevel chosen in addition to being written in the logfile. Accepted values are true, True, on, yes, false, False, off, no. Default value: false
    loglevel: INFO                              # The log level which should be applied for log messages. Accepted values are DEBUG, INFO, WARN, ERROR, CRITICAL. It is not recommended to enable DEBUG for performance reasons.
    env:                                        # Environment variables for taskmasterd. Processes will inherit them.
        username: john
```

#### socket:

```yaml
socket:                                         # UNIX socket configuration used to communicate between taskmasterd and taskmasterctl
    enable: true                                # Determines whether a socket should be created. Accepted values are true, True, on, yes, false, False, off, no. Default value: false
    socketpath: /tmp/taskmaster.sock            # The path to the UNIX socket. This path will also be used for taskmasterctl. It is recommended to use absolute paths.
    umask: 022                                  # Umask used for the creation of the socket in octal. This will determine who has the rights to communicate with taskmasterd. Accepted values range from 0 to 0777 (in octal). Default value: 022 (in octal)
    uid: john                                   # User who will own the UNIX socket
    gid: adm                                    # Group which will own the UNIX socket
    username: user                              # Username for authentication
    password: <HASH>                            # Hash of password, get it by using ./taskmasterd -x <PASSWORD>
```
