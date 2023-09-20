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

>exit        |Exit the taskmaster shell.

>maintail -f     |Continuous tail of taskmaster main log file (Ctrl-D to exit)
>maintail -100   |last 100 *bytes* of taskmaster main log file
>maintail        |last 1600 *bytes* of taskmaster main log file

>quit    |Exit the taskmaster shell.

>signal \<signal name> \<name>           |Signal a process
>signal \<signal name> \<name> \<name>   |Signal multiple processes or groups
>signal \<signal name> all               |Signal all processes

>stop \<name>            |Stop a process
>stop \<name> \<name>    |Stop multiple processes or groups
>stop all                |Stop all processes

>fg \<process>   |Connect to a process in foreground mode
        Ctrl-D to exit

>reload          |Restart the remote taskmasterd.

>restart \<name>             |Restart a process
>restart \<name> \<name>     |Restart multiple processes or groups
>restart all                 |Restart all processes

>start \<name>               |Start a process
>start \<name> \<name>       |Start multiple processes or groups
>start all                   |Start all processes

>tail [-f] \<name> [stdout|stderr]   (default stdout)
Ex:
>tail -f \<name>         |Continuous tail of named process stdout
>        Ctrl-D to exit
>tail -100 \<name>       |last 100 *bytes* of process stdout
>tail \<name> stderr     |last 1600 *bytes* of process stderr

>help                |Print a list of available commands
>help \<command>     |Print help for \<command>

>pid             |Get the PID of taskmasterd.
>pid \<name>     |Get the PID of a single child process by name.
>pid all         |Get the PID of every child process, one per line.

>shutdown        |Shut the remote taskmasterd down.

>status \<name>          |Get status for a single process
>status \<name> \<name>  |Get status for multiple named processes
>status                  |Get all process status info

>update          |Reload config and add/remove as necessary, and will restart affected programs
>update all      |Reload config and add/remove as necessary, and will restart affected programs
