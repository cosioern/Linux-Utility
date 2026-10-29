#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <regex.h>
#include <string.h>
#include <ncurses.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <stdbool.h>
#include <limits.h>

#define BUFFER_SIZE 4096

// prototypes
int snapshot(void);
int process_display(pid_t);

// main
int main(int argc, char* argv[]) {

    printf("Linux Utility!\n\n");

    int opt;

    while ((opt = getopt(argc, argv, "p:fse:")) != -1) {

        switch (opt) {

            // snapshot (number of processes, memory usage, CPU usage)
            case 's':
                snapshot();
                break;

            // case for continuous single process display?
            case 'p':
                char *endptr;
                errno = 0;
                long val = strtol(optarg, &endptr, 10);
                // error checking for valid PID
                if (errno == ERANGE || endptr == optarg || *endptr != '\0' || val <= 0 || val > INT_MAX) {
                    printf("Bad PID given as input!\n");
                    return EXIT_FAILURE;
                }

                pid_t pid = (pid_t)val;

                // printf("Process ID:\t %d\n", pid);
                if (process_display(pid) == EXIT_FAILURE) {
                    printf("process_display failed with pid:%d\n", pid);
                    return EXIT_FAILURE;
                }
                break;

            // case for continuous all process display?

            // case for process diagnostic?

            // sanity check option - echo back string
            case 'e':
                printf("Your input: %s\n", optarg);
                break;

            case '?':
                printf("Usage: %s [options] \n", argv[0]);
                printf(" -s             System Snapshot\n");
                printf(" -p  PID        Single Process Display\n");
                printf(" -e  String     Echo Back Input\n");
                return EXIT_FAILURE;

            default:
                abort();
        }

    }


    return EXIT_SUCCESS;
}

////////////////////////////////////////////////////////
int process_display(pid_t pid) {

    // report error on lookup if process is of a different user (print current user and which user has appropriate permissions)
    FILE *stat;
    FILE *status;
    FILE *global_stats;

    char statPath[64] = "/proc/";
    char statusPath[64] = "/proc/";
    char buffer[BUFFER_SIZE];

    struct passwd *pw;
    struct group *gr;
    char *token;
    int index = 0;
    char *endptr;

    time_t t;
    time_t btime = 0;
    time_t elapsed;
    unsigned long long start_time = 0;

    // open /proc/[PID]/status
    snprintf(statusPath, sizeof(statusPath), "/proc/%d/status", pid);

    status = fopen(statusPath, "r");
    if (status == NULL) {
        printf("Unable to open /proc/%d/status", pid);
        return EXIT_FAILURE;
    }

    // print process details (name, user, and group)
    while (fgets(buffer, sizeof(buffer), status) != NULL) {
        // printf("%s\n", buffer);
        if (strncmp(buffer, "Name:", 5) == 0) {
            printf("%s", buffer);
        }
        else if (strncmp(buffer, "Uid:", 4) == 0) {
            token = strtok(buffer, " \t\n");
            token = strtok(NULL, " \t\n");
            pw = getpwuid(atoi(token));
            printf("User: %s\n", pw ? pw->pw_name : "Unknown User");
        }
        else if (strncmp(buffer, "Gid:", 4) == 0) {
            token = strtok(buffer, " \t\n");
            token = strtok(NULL, " \t\n");
            gr = getgrgid(atoi(token));
            printf("Group: %s\n", gr ? gr->gr_name : "Unknown Group");
        }
        else if (strncmp(buffer, "State:", 6) == 0) {
            printf("%s", buffer);
        }
    }

    // display time of process - remove name finding logic!!!!!!!!!
    snprintf(statPath, sizeof(statPath), "/proc/%d/stat", pid);
    stat = fopen(statPath, "r");
    if (stat == NULL) {
        printf("Unable to open /proc/%d/stat", pid);
        fclose(status);
        return EXIT_FAILURE;
    }

    // move process stats into buffer
    if (fgets(buffer, sizeof(buffer), stat) == NULL) {
        printf("Unexpectedly empty /proc/%d/stat", pid);
        fclose(status);
        fclose(stat);
        return EXIT_FAILURE;
        // printf("Found Process Stats: \n%s\n", buffer);
    }

    //----------------------------- cut out 
    char *end = strrchr(buffer, ')');
    token = strtok(end+1, " ");
    // // first token is a letter
    // printf("Process State: %s\n", token);
    token = strtok(NULL, " ");
    while (index <= 18) {
        if (index++ == 18) {
            printf("Process Start Tick: %ld\n", strtol(token, &endptr, 10));
            break;
        }
        token = strtok(NULL, " ");
    }
    start_time = strtoull(token, &endptr, 10);

    // open /proc/stat for global system statistics
    global_stats = fopen("/proc/stat", "r");
    if (global_stats == NULL) {
        perror("Could not find global stats file");
        fclose(status);
        fclose(stat);
        return EXIT_FAILURE;
    }

    // parse /proc/stat for btime (total accumulated CPU time)
    while (fgets(buffer, BUFFER_SIZE, global_stats) != NULL) {
        if (strncmp(buffer, "btime", 5) == 0) {
            token = strtok(buffer, " ");
            token = strtok(NULL, " \n");
            if (token == NULL) {
                perror("Failed to find btime\n");
                return EXIT_FAILURE;
            }

            btime = strtol(token, &endptr, 10);
            // printf("numberL %ld", t);
            if (endptr == token || *endptr != '\0') {
                perror("Failed to parse /proc/stat btime\n");
                fclose(status);
                fclose(stat);
                fclose(global_stats);
                return EXIT_FAILURE;
            }

            break;
        }
    }

    // full CPU time
    t = btime + start_time / sysconf(_SC_CLK_TCK);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));
    printf("CPU Start Time: %s\n", buf);

    // process elapsed time
    elapsed = time(NULL) - t;
    printf("Running: %ldd %ldh %ldm %lds\n",
       elapsed / 86400, (elapsed % 86400) / 3600,
       (elapsed % 3600) / 60, elapsed % 60);


    // buttons to kill / diagnose / track
    // who the process belongs to (find parent or process tree)
    // display if process is blocked, sleeping, etc

    fclose(status);
    fclose(stat);
    fclose(global_stats);

    return EXIT_SUCCESS;
}


////////////////////////////////////////////////////////
int snapshot(void) {

    DIR *dir;
    struct dirent *entry;
    int num_processes = 0;
    double cpu_usage = 0.0;
    // double mem_usage = 0.0;
    regex_t regex;
    FILE *file;
    char buffer[256];
    char* token;

    // user + nice + system + irq + softirq + steal
    long long busy1 = 0; 
    long long busy2 = 0;
    // total = busy + idle + iowait
    long long total1 = 0;
    long long total2 = 0;
    // CPU values are: user (1), nice (2), system (3), idle (4), iowait (5), irq (6), softirq (7), steal (8), guest (9), guest_nice (10)
    int index = 0;

    // open /proc directory
    dir = opendir("/proc");
    if (dir == NULL) {
        perror("Unable to open /proc");
        return EXIT_FAILURE;
    }
    // compile regex to match numerical directory names (process IDs)
    if (regcomp(&regex, "[0-9]+$", REG_EXTENDED) != 0) {
        perror("Unable to compile regex");
        return EXIT_FAILURE;
    }

    // count number of running processes (numerical directories in /proc)
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            if (regexec(&regex, entry->d_name, 0, NULL, 0) == 0) {
                num_processes++;
            }
        }
    }
    printf("Number of Processes: %d\n", num_processes);

    // calculate CPU % usage ~ 100 * (busy2 - busy1) / (total2 - total1)
    file = fopen("/proc/stat", "r");
    if (file == NULL) {
        perror("Unable to open /proc/stat");
        return EXIT_FAILURE;
    }

    // find the cpu line in /proc/stat
    regfree(&regex);
    if (regcomp(&regex, "^cpu ", REG_EXTENDED) != 0) {
        perror("Unable to compile regex");
        return EXIT_FAILURE;
    }
    
    // read /proc/stat and find CPU line to parse for CPU usage values
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        if (regexec(&regex, buffer, 0, NULL, 0) == 0) {
            token = strtok(buffer, " ");
            while (token != NULL) {
                // user (1), nice (2), system (3), irq (6), softirq (7), steal (8)
                if (index == 1 || index == 2 || index == 3 || index == 6 || index == 7 || index == 8) {
                    busy1 += strtoull(token, NULL, 10);
                }
                // user (1), nice (2), system (3), idle (4), iowait (5), irq (6), softirq (7), steal (8)
                if (index == 1 || index == 2 || index == 3 || index == 4 || index == 5 || index == 6 || index == 7 || index == 8) {
                    total1 += strtoull(token, NULL, 10);
                }
                token = strtok(NULL, " ");
                index++;
            }
        }
    }

    fclose(file);
    sleep(1);

    // repeat for second reading of CPU values
    file = fopen("/proc/stat", "r");
    if (file == NULL) {
        perror("Unable to open /proc/stat");
        return EXIT_FAILURE;
    }
    token = NULL;
    index = 0;
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        if (regexec(&regex, buffer, 0, NULL, 0) == 0) {
            token = strtok(buffer, " ");
            while (token != NULL) {
                if (index == 1 || index == 2 || index == 3 || index == 6 || index == 7 || index == 8) {
                    busy2 += strtoull(token, NULL, 10);
                }
                if (index == 1 || index == 2 || index == 3 || index == 4 || index == 5 || index == 6 || index == 7 || index == 8) {
                    total2 += strtoull(token, NULL, 10);
                }
                token = strtok(NULL, " ");
                index++;
            }
        }
    }
    cpu_usage = 100.0 * (busy2 - busy1) / (total2 - total1);
    printf("CPU Usage: %.2f%%\n", cpu_usage);
    
    // open /proc/meminfo and prepare regex
    int mem_total = 0;
    int mem_available = 0;
    file = fopen("/proc/meminfo", "r");
    if (file == NULL) {
        perror("Unable to open /proc/meminfo");
        return EXIT_FAILURE;
    }
    regfree(&regex);
    if (regcomp(&regex, "^MemTotal:|^MemAvailable:", REG_EXTENDED) != 0) {
        perror("Unable to compile regex");
        return EXIT_FAILURE;
    }
    
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        if (regexec(&regex, buffer, 0, NULL, 0) == 0) {
            token = strtok(buffer, " ");
            while (token != NULL) {
                if (strcmp(token, "MemTotal:") == 0) {
                    token = strtok(NULL, " ");
                    mem_total = atoi(token);
                }
                else if (strcmp(token, "MemAvailable:") == 0) {
                    token = strtok(NULL, " ");
                    mem_available = atoi(token);
                    break;
                }
                token = strtok(NULL, " ");
            }
        }
    }

    printf("Memory Usage: %.2f%%\n", (float)(mem_total - mem_available) / mem_total * 100);

    if (closedir(dir) != 0) {
        perror("Unable to close /proc");
        return EXIT_FAILURE;
    }
    regfree(&regex);
    fclose(file);
    return EXIT_SUCCESS;
}