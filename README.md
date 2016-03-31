# unix_signals

Build:
make

Run:
./bin/lab
Without parameters --mode=std will be used by default.

Run with parameters:

1. ./bin/lab --mode=std
Handling standart signals(SIGUSR1, SIGUSR2, SIGHUP)
Output: SIGNO, PID and GID of sending process.

2. ./bin/lab --mode=posix --amount=N
Generating and handling random realtime signals. N - count of signals to generate
Output of receiver(parent) process:
|  N  | MYPID | CHILD | SIGNO | VALUE |
Output of transmitter(child) process:
|  N  | MYPID | PARENT | SIGNO | VALUE |

3. ./bin/lab --mode=pipe
Handling SIGPIPE signal. 

4. ./bin/lab --mode=kill --signal=SIGNO --pid=PID
Sending signal SIGNO to process with id PID

5. ./bin/lab --mode=child
Handling SIGCHLD signal.
Output:
Values of siginfo_t structure fields.

Clean:
make clean
