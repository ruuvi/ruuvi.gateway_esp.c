Posix GCC Eclipse FreeRTOS 6.0.4 Simulator

Contributed by William Davy. william.davy@wittenstein.co.uk

Changelog:
	10/04/2010 - Added code for the Asynchronous Serial example. Refactored the 
generic Asynchronous module. Added support for the Run-time Statistics. Integrated the
latest FreeRTOS code base. (Fixed the Eclipse GDB debug issues, see .gdbinit settings).
	09/10/2009 - Added code and example tasks for Asynchronous IO specifically for
Posix Message Queues (IPC) and UDP packets.
	22/06/2009 - Fixed a bug regarding re-enabling of interrupts when a
task previously had them disabled.
	- Implemented vPortSetInterruptMaskFromISR and
vPortClearInterruptMaskFromISR to enable the use of FreeRTOS system calls from
interrupt handlers (Signal handlers which interact with queues).
	- Changed the Signal Handler masks so that other signals can be used 
without having to change port layer code and signals will act more like
interrupts.
	- Changed the tick to be the real-time tick and added code in main.c 
that makes the process far more agreeable. The process will no longer steal all
of the host OS idle time but will still run in pseudo real-time.


Introduction:
This is a port that allows FreeRTOS to act as a scheduler for pthreads within a process.
It is designed to allow for development and testing of code in a Posix environment. It 
is considered a simulator because it will not keep real-time but it will retain the same
deterministic task switching.

Build Instructions
	Pre-requisites:
		make	(tested with GNU Make 3.81)
		gcc	(tested with gcc 4.4.3)
	Optional:
		Eclipse Galileo
		CDT 6.0
		Eclipse STATEVIEWER Plug-in v1.0.10

If you have Eclipse and the CDT installed then you can simply import the project (copying
into the workspace or not). The project has Debug and Release build configurations and is
a Managed-make C executable project so the Project Properties define the contents of the
Makefile.

Alternatively, to build the executable without Eclipse and the CDT, simply 'cd' into the
Debug or Release directory and type 'make all'.

The compilation and execution have been tested in an IA64 Ubuntu 8.10 and i386 Debian 4.0
build environments (using standard packages). No changes are necessary between the 64 bit
and 32 bit builds. The Debian machine is a 667MHz and it is able to run the simulator 
successfully, even with the 50 or so tasks within the demo.

Debugging using GDB.
The port layer makes use of process signals. The four process signals that are used are
SIGUSR1, SIGUSR2, SIGIO and SIGALRM. If a pthread is not waiting on the signal then GDB will
pause the process on receipt of the signal. GDB must be told to ignore (and not print) the
signal SIGUSR1 because it is received asynchronously by each thread. In GDB type 'handle
SIGUSR1 nostop noprint pass' to ensure that the debugging is not interrupted by the signals.
The equivalent in Eclipse is achieved by using the 'Signals' view and getting the properties
of SIGUSR1 and de-selecting the suspend option. See 'man signal' for more information.

Alternatively, create a file in your home directory called .gdbinit and place the following
two lines in it:
handle SIGUSR1 nostop noignore noprint
handle SIG34 nostop noignore noprint

Adding the two lines above to the .gdbinit file will tell GDB to not break on those signals.
It may be necessary to change the Eclipse Debug configuration to point at the file ~/.gdbinit

If necessary, SIGUSR1 and SIGUSR2 can be re-defined to any other unused signals (such as the
real-time signals).

There are three different timers available for use as the System tick; ITIMER_REAL,
ITIMER_VIRTUAL and ITIMER_PROF. The default timer is ITIMER_REAL because it most closely
matches the real-world timing. ITIMER_VIRTUAL only counts when the process is executing in
user space, therefore, the timer will stop when a break-point is hit. ITIMER_PROF is
equivalent to ITIMER_VIRTUAL but it includes time executing system calls. ITIMER_REAL
continues counting even when the process is not executing at all, therefore, it represents
real-time. ITIMER_REAL is the only sensible option because the other timers don't tick unless
the process is actually running, hence, if nanosleep is called in the IDLE task hook, the 
time will hardly ever increase with the non-real timers.

One of the big advantages of debugging using Eclipse and the simulator is that you get a 
separate thread/task listing for each task you create. This means that you can inspect the 
call stack of any task when paused in the debugger, even if it is not the currently excuting
task. If you also use the Eclipse STATEVIEWER Plug-in available from the Downloads section of
www.HighIntegritySystems.com then you get a little bit of FreeRTOS Task scheduler information
and Posix Threads call stacks. The STATEVIEWER Plug-in is shipped in a Windows Installer but
that installer works in WINE and you simply need to ensure that the rtos.openrtos.viewer jar
file is placed in your Eclipse/plugins directory.

Please note that the demo includes 50 or so tasks so it is quite processor intensive. The 
projects where I use this port for testing, have approximately 10 tasks and all of the 
IO and subsequent processing is event driven so the process as a whole spends 99% of its time 
in the IDLE task performing the nanosleep (which doesn't consume any processing time).

Asynchronous IO
Provided with the demo are two examples of Asynchronous IO that can be used for implementing
event driven communicaiton. Asynchronous IO is important because it means that the processing
time is not consumed by polling for the next message (especially important in this simulator 
because you don't want to steal processing time from the other processes unless there is some
work to be done). The three AsyncIO examples that have been provided are Posix Message Queues, 
UDP sockets and Serial communication (/dev/ttyS0).

The Posix Message Queues can be used for Inter-Process Communication on a single host. A 
queue is created by passing a Queue name. Processes communicate by opening a handle to the 
Queue by passing the same string name for that queue. Packets are then sent and received via 
the file descriptor. When a packet is written to a queue and there is a task waiting to receive
the packet, a signal is sent to the task and the registered signal handler picks up the packet 
and delivers it via a FreeRTOS Queue to the task that is waiting on that Queue.
Type 'man mq_overview' in the terminal to find out more about Message Queues.

The UDP AsyncIO simply uses the BSD sockets interface but registers a signal handler that 
handles SIGIO signals which indicate that a packet is waiting to be read from a particular
socket. When opening a socket, a callback function can be registered which is called when there
is a packet waiting to be read. An example callback function is provided which takes a FreeRTOS
queue as a parameter and delivers the received packet to that queue. Sockets can also be opened 
that are for sending only, which is done by passing NULL parameters to the open function.

The Serial communication example is very simple. It configures /dev/ttyS0 to be a RAW 38400 
serial pipe and for each character that is received, it is echoed to the local console. The 
code should be enhanced with error correction and packet transmission so that it works in a 
similar method to the UDP example. Note that it is possible to add software flow control for
better results, see 'man termios' for more details.

In theory, the code provided for the AsyncIO can be extended to provide asynchronous IO on 
all file descriptors that support it.

I use two of the AsyncIO mechanisms as alternatives to the CAN bus. I can execute a CAN-Master 
and CAN-Slave on the same machine as two different instances (processes) of the simulator and 
they communicate via two Posix Message Queues. I can easily debug the CAN-Master/Slave using 
Eclipse and gdb. On top of that, I can use gprof and gcov(lcov), to profile and find code 
coverage of the tasks. Finally, I use the UDP sockets to provide a broadcast mechanism by 
sending packets to the local subnet, and all simulators on the all machines receive those 
and broadcast their replies. As such, the UDP broadcast provides the bus in CAN bus.

The send/receive tasks interact using FreeRTOS queues with CAN message objects so it is simple 
to replace the communication driver without any impact on the application code. In fact, the 
real CAN bus driver, on the embedded platform, is interrupt driven and delivers its messages
to a FreeRTOS Queue. The use of signals in this Simulator mirrors the use of interrupts in 
the real world applications.

Port-Layer Design Methodology Justification
A simple implementation of a FreeRTOS Simulator would simply wrap the platform native threads
and calls to switch Task contexts would call the OS suspend and resume thread API. This
simulator uses the Posix signals as a method of controlling the execution of the underlying
Posix threads. Signals can be delivered to the threads asynchronously so they interrupt the
execution of the target thread, whilst suspended threads wait for synchronous signals to
resume.

Typically, when designing a multi-threaded process, the multiple threads are used to allow for
concurrent execution and to implement a degree of non-blocking on IO tasks. This simulator uses
the threads not for their concurrent execution but solely to store the context of the execution.
Signals and mutexes are used to synchronise the switching of the context but ultimately, the
choice to change the context is driven by the FreeRTOS scheduler.

When a new Task is created, a pthread is created as the context for the execution of that Task.
The pthread immediately suspends itself, returning the execution to the creator. When a pthread
is suspended it is waiting in a call to 'sigwait' which is blocked until it receives a resume
signal.

FreeRTOS Tasks can be switched in two manners, co-operatively by calling 'taskYIELD()' or
pre-emptively as part of the System Tick. In this simulator, the Task contexts are switched by
resuming the next task context (decided by the FreeRTOS Scheduler) and suspending the current
context (with a brief handshake between the two).

The System tick is generated using an ITIMER and the signal is delivered (only to) the currently
executing pthread. The System Tick Signal Handler increments the tick and selects the next
context, resuming that thread and sending a signal to itself to suspend (which is only processed
when exiting the System Tick Signal Handler as signals are queued).

Known Issues
The pthread_create and pthread_exit/cancel are system intensive calls which can rapidly saturate
the processing time. The Common Demo code includes a Suicidal Tasks test which executes
successfully, however, if the test is the only test which is being executed the process slowly
grinds to a halt.

To prevent the process from stealing all of the Idle excution time of the Host OS, nano_sleep()
can be used because it doesn't use any signals in its implementation but will abort from the 
sleep/suspending the process immediately to service a signal. Therefore, the best way to use it
is to set a sleep time longer than a FreeRTOS execution time slice and call it from the Idle task
so that the process suspends until the next tick.

All feedback is welcome.

