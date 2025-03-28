/*
 * tsh - A tiny shell program with job control
 *
 * <Put your name and login ID here>
 * Jay
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

 /* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

 /* Global variables */
extern char** environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t
{              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char* cmdline);
int builtin_cmd(char** argv);
void do_bgfg(char** argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char* cmdline, char** argv);
void sigquit_handler(int sig);

void clearjob(struct job_t* job);
void initjobs(struct job_t* jobs);
int maxjid(struct job_t* jobs);
int addjob(struct job_t* jobs, pid_t pid, int state, char* cmdline);
int deletejob(struct job_t* jobs, pid_t pid);
pid_t fgpid(struct job_t* jobs);
struct job_t* getjobpid(struct job_t* jobs, pid_t pid);
struct job_t* getjobjid(struct job_t* jobs, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t* jobs);

void usage(void);
void unix_error(char* msg);
void app_error(char* msg);
typedef void handler_t(int);
handler_t* Signal(int signum, handler_t* handler);

void reach()
{
    printf("%d reaches here\n", getpid());
    fflush(stdout);
}

/*
 * main - The shell's main routine
 */
int main(int argc, char** argv)
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF)
    {
        switch (c)
        {
        case 'h':             /* print help message */
            usage();
            break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
            break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
            break;
        default:
            usage();
        }
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT, sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler);

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1)
    {

        /* Read command line */
        if (emit_prompt)
        {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin))
        { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    }

    exit(0); /* control never reaches here */
}

/*
 * eval - Evaluate the command line that the user has just typed in
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
*/
void eval(char* cmdline)
{
    char* argv[MAXLINE];
    int background = parseline(cmdline, argv);
    if (builtin_cmd(argv))
    {
        return;
    }

    // replace \n
    cmdline[strlen(cmdline) - 1] = ' ';

    pid_t pid;
    sigset_t all_signal, child_signal, previous_signal;
    sigfillset(&all_signal);
    sigemptyset(&child_signal);
    sigaddset(&child_signal, SIGCHLD);

    // block SIGCHLD
    sigprocmask(SIG_BLOCK, &child_signal, &previous_signal);

    // child process
    if ((pid = fork()) == 0)
    {
        // restore signal in child process
        sigprocmask(SIG_SETMASK, &previous_signal, NULL);
        // create another process group
        setpgid(0, 0);
        if (execve(argv[0], argv, environ) < 0)
        {
            printf("%s: Command not found\n", argv[0]);
            exit(0);
        }
    }


    // block every signals to protect addjob function
    sigprocmask(SIG_BLOCK, &all_signal, NULL);

    addjob(jobs, pid, (background ? BG : FG), cmdline);

    // restore previous signals
    sigprocmask(SIG_SETMASK, &previous_signal, NULL);

    if (background)
    {
        printf("[%d] (%d) %s\n", pid2jid(pid), pid, cmdline);
        // unblock child signal
        sigprocmask(SIG_UNBLOCK, &child_signal, NULL);
        return;
    }

    // wait for foreground process
    waitfg(pid);
    // unblock child signal
    sigprocmask(SIG_UNBLOCK, &child_signal, NULL);

    return;
}

/*
 * parseline - Parse the command line and build the argv array.
 *
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.
 */
int parseline(const char* cmdline, char** argv)
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char* buf = array;          /* ptr that traverses command line */
    char* delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf) - 1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'')
    {
        buf++;
        delim = strchr(buf, '\'');
    }
    else
    {
        delim = strchr(buf, ' ');
    }

    while (delim)
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* ignore spaces */
            buf++;

        if (*buf == '\'')
        {
            buf++;
            delim = strchr(buf, '\'');
        }
        else
        {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;

    if (argc == 0)  /* ignore blank line */
        return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
    {
        argv[--argc] = NULL;
    }
    return bg;
}

/*
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.
 */
int builtin_cmd(char** argv)
{
    const char* str = argv[0];
    if (strcmp(str, "quit") == 0)
    {
        exit(0);
    }
    else if (strcmp(str, "fg") == 0 || strcmp(str, "bg") == 0)
    {
        do_bgfg(argv);
    }
    else if (strcmp(str, "jobs") == 0)
    {
        listjobs(jobs);
    }
    else
    {
        return 0;
    }
    return 1;
}

/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char** argv)
{
    const char* command = argv[0], * str_id = argv[1];
    if (command == NULL || str_id == NULL)
    {
        printf("%s command requires PID or %%jobid argument\n", command);
        return;
    }
    int int_id;
    if (str_id[0] == '%')
    {
        int_id = strtol(str_id + 1, NULL, 10);
    }
    else
    {
        int_id = strtol(str_id, NULL, 10);
    }
    if (int_id == 0)
    {
        printf("%s: argument must be a PID or %%jobid\n", command);
        return;
    }
    // find target job
    struct job_t* target_job;
    target_job = getjobjid(jobs, int_id);

    // can't find target job
    if (target_job == NULL)
    {
        if (str_id[0] == '%')
        {
            printf("%%%d: No such job\n", int_id);
        }
        else
        {
            printf("(%d): No such process\n", int_id);
        }
        return;
    }
    if (strcmp(command, "fg") == 0)
    {
        if (fgpid(jobs) != 0)
        {
            printf("another foregound job exists\n");
            return;
        }
        if (target_job->state != ST && target_job->state != BG)
        {
            printf("can't change to foreground\n");
            return;
        }

        // change target job to foreground process
        target_job->state = FG;
        kill(target_job->pid, SIGCONT);
        waitfg(target_job->pid);
    }
    else if (strcmp(command, "bg") == 0)
    {
        if (target_job->state != ST && target_job->state != BG)
        {
            printf("can't change to background");
            return;
        }

        // change target job to background process
        target_job->state = BG;
        kill(target_job->pid, SIGCONT);
        printf("[%d] (%d) %s\n", target_job->jid, target_job->pid, target_job->cmdline);
    }
    else
    {
        printf("bgfg error");
        return;
    }
    return;
}

/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    // wait for foreground job to terminate
    struct job_t* foreground_job = getjobpid(jobs, pid);
    if (foreground_job == NULL)
    {
        return;
    }
    sigset_t no_block;
    sigemptyset(&no_block);
    while (foreground_job->state == FG)
    {
        //printf("waiting for signals...\n");
        //fflush(stdout);
        sigsuspend(&no_block);
        //reach();
        //printf("foreground job state: %d\n", foreground_job->state);
        //fflush(stdout);
    }
}

/*****************
 * Signal handlers
 *****************/

 /*
  * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
  *     a child job terminates (becomes a zombie), or stops because it
  *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
  *     available zombie children, but doesn't wait for any other
  *     currently running children to terminate.
  */
void sigchld_handler(int sig)
{
    int olderrno = errno;
    sigset_t all_signal, previous;
    // block all signal
    sigfillset(&all_signal);
    sigprocmask(SIG_BLOCK, &all_signal, &previous);

    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        // exit normally
        if (WIFEXITED(status))
        {
            deletejob(jobs, pid);
        }
        // terminated by a signal
        else if (WIFSIGNALED(status))
        {
            printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), pid, WTERMSIG(status));
            deletejob(jobs, pid);
        }
        // stopped by a signal
        else if (WIFSTOPPED(status))
        {
            printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid, WSTOPSIG(status));
            getjobpid(jobs, pid)->state = ST;
        }
    }

    errno = olderrno;
    // restore previous signal
    sigprocmask(SIG_SETMASK, &previous, NULL);
    return;
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */
void sigint_handler(int sig)
{
    int olderrno = errno;
    sigset_t all_signal, previous;
    // block all signal
    sigfillset(&all_signal);
    sigprocmask(SIG_BLOCK, &all_signal, &previous);

    pid_t pid = fgpid(jobs);
    if (pid == 0)
    {
        return;
    }
    kill(-pid, SIGINT);

    errno = olderrno;
    // restore previous signal
    sigprocmask(SIG_SETMASK, &previous, NULL);
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void sigtstp_handler(int sig)
{
    int olderrno = errno;
    sigset_t all_signal, previous;
    // block all signal
    sigfillset(&all_signal);
    sigprocmask(SIG_BLOCK, &all_signal, &previous);

    pid_t pid = fgpid(jobs);
    if (pid == 0)
    {
        return;
    }
    kill(pid, SIGTSTP);

    errno = olderrno;
    // restore previous signal
    sigprocmask(SIG_SETMASK, &previous, NULL);
    return;
}

/*********************
 * End signal handlers
 *********************/

 /***********************************************
  * Helper routines that manipulate the job list
  **********************************************/

  /* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t* job)
{
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t* jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t* jobs)
{
    int i, max = 0;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid > max)
            max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t* jobs, pid_t pid, int state, char* cmdline)
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == 0)
        {
            jobs[i].pid = pid;
            jobs[i].state = state;
            jobs[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(jobs[i].cmdline, cmdline);
            if (verbose)
            {
                printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t* jobs, pid_t pid)
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == pid)
        {
            clearjob(&jobs[i]);
            nextjid = maxjid(jobs) + 1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t* jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].state == FG)
            return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t* getjobpid(struct job_t* jobs, pid_t pid)
{
    int i;

    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
            return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t* getjobjid(struct job_t* jobs, int jid)
{
    int i;

    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid == jid)
            return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid)
{
    int i;

    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
        {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t* jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid != 0)
        {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
            switch (jobs[i].state)
            {
            case BG:
                printf("Running ");
                break;
            case FG:
                printf("Foreground ");
                break;
            case ST:
                printf("Stopped ");
                break;
            default:
                printf("listjobs: Internal error: job[%d].state=%d ",
                    i, jobs[i].state);
            }
            printf("%s\n", jobs[i].cmdline);
        }
    }
}
/******************************
 * end job list helper routines
 ******************************/


 /***********************
  * Other helper routines
  ***********************/

  /*
   * usage - print a help message
   */
void usage(void)
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char* msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char* msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t* Signal(int signum, handler_t* handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}



