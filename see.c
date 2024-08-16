#include <ApplicationServices/ApplicationServices.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>

#define INTERVAL 600  // log data every 10 mins
#define HOURS_TO_SECONDS(x) ((x) * 3600)
#define DATA_POINTS (HOURS_TO_SECONDS(24) / 600)
#define PIXELS_TO_METERS(x) ((x) * 0.0002645833)  // conversion factor for pixels to meters (assumes 96 DPI)

// struct to hold activity data for single interval
typedef struct {
    time_t timestamp;
    int keypresses;
    double mouse_moves;  // store mouse movement [m]
    int left_clicks;
    int right_clicks;
    int middle_clicks;
} ActivityData;

ActivityData data = {0};
ActivityData history[DATA_POINTS] = {0};
int currentIndex = 0;

// cumulative counts
unsigned long long cumulative_keypresses = 0;
double cumulative_mouse_moves = 0.0;
unsigned long long cumulative_left_clicks = 0;
unsigned long long cumulative_right_clicks = 0;
unsigned long long cumulative_middle_clicks = 0;

double last_mouse_x = -1.0, last_mouse_y = -1.0;

CGEventRef eventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    if (type == kCGEventKeyDown) {
        data.keypresses++;
        cumulative_keypresses++;
    } else if (type == kCGEventLeftMouseDown) {
        data.left_clicks++;
        cumulative_left_clicks++;
    } else if (type == kCGEventRightMouseDown) {
        data.right_clicks++;
        cumulative_right_clicks++;
    } else if (type == kCGEventMouseMoved) {
        CGPoint mouse_pos = CGEventGetLocation(event);
        if (last_mouse_x >= 0 && last_mouse_y >= 0) {
            double dx = mouse_pos.x - last_mouse_x;
            double dy = mouse_pos.y - last_mouse_y;
            double distance = sqrt(dx * dx + dy * dy);
            double distance_meters = PIXELS_TO_METERS(distance);
            data.mouse_moves += distance_meters;
            cumulative_mouse_moves += distance_meters;
        }
        last_mouse_x = mouse_pos.x;
        last_mouse_y = mouse_pos.y;
    } else if (type == kCGEventOtherMouseDown) {
        data.middle_clicks++;
        cumulative_middle_clicks++;
    }

    return event;
}

void logDataToFile() {
    // log cumulative counts
    FILE *cumulativeFile = fopen("cumulative_data.csv", "w");
    if (cumulativeFile) {
        fprintf(cumulativeFile, "event,count\n");
        fprintf(cumulativeFile, "keypresses,%llu\n", cumulative_keypresses);
        fprintf(cumulativeFile, "mousemoves,%.2f meters\n", cumulative_mouse_moves);
        fprintf(cumulativeFile, "leftclicks,%llu\n", cumulative_left_clicks);
        fprintf(cumulativeFile, "rightclicks,%llu\n", cumulative_right_clicks);
        fprintf(cumulativeFile, "middleclicks,%llu\n", cumulative_middle_clicks);
        fclose(cumulativeFile);
        printf("cumulative data logged successfully!\n");
    } else {
        perror("error opening cumulative_data.csv");
    }

    // log past 24 hours
    FILE *past24HoursFile = fopen("past_24_hours_data.csv", "w");
    if (past24HoursFile) {
        unsigned long long past24_keypresses = 0;
        double past24_mouse_moves = 0.0;
        unsigned long long past24_left_clicks = 0;
        unsigned long long past24_right_clicks = 0;
        unsigned long long past24_middle_clicks = 0;

        fprintf(past24HoursFile, "timestamp,keypresses,mousemoves,leftclicks,rightclicks,middleclicks\n");
        for (int i = 0; i < DATA_POINTS; i++) {
            int index = (currentIndex + i) % DATA_POINTS;
            if (history[index].timestamp != 0) {
                fprintf(past24HoursFile, "%ld,%d,%.2f,%d,%d,%d\n",
                        history[index].timestamp,
                        history[index].keypresses,
                        history[index].mouse_moves,
                        history[index].left_clicks,
                        history[index].right_clicks,
                        history[index].middle_clicks);

                past24_keypresses += history[index].keypresses;
                past24_mouse_moves += history[index].mouse_moves;
                past24_left_clicks += history[index].left_clicks;
                past24_right_clicks += history[index].right_clicks;
                past24_middle_clicks += history[index].middle_clicks;
            }
        }

        // append cumulative summary for 24h
        fprintf(past24HoursFile, "cumulative, %llu, %.2f meters, %llu, %llu, %llu\n",
                past24_keypresses, past24_mouse_moves,
                past24_left_clicks, past24_right_clicks, past24_middle_clicks);

        fclose(past24HoursFile);
    }
    // printf("Current cumulative values: keypresses=%llu, mousemoves=%.2f, leftclicks=%llu, rightclicks=%llu, middleclicks=%llu\n",
    //        cumulative_keypresses, cumulative_mouse_moves, cumulative_left_clicks, cumulative_right_clicks, cumulative_middle_clicks);
}

// updating git
void update_github() {
    time_t now = time(NULL);
    char timestamp[26];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    char commit_message[100];
    snprintf(commit_message, sizeof(commit_message), "update event data - %s", timestamp);

    printf("attempting to update github @ %s...\n", timestamp);

    int result = system("git add cumulative_data.csv past_24_hours_data.csv");
    if (result != 0) {
        fprintf(stderr, "error adding files to git\n");
        return;
    }

    char git_commit_command[150];
    snprintf(git_commit_command, sizeof(git_commit_command), "git commit -m \"%s\"", commit_message);
    result = system(git_commit_command);
    if (result != 0) {
        fprintf(stderr, "error committing changes\n");
        return;
    }

    result = system("git push origin main");
    if (result != 0) {
        fprintf(stderr, "error pushing to git\n");
        return;
    }
    printf("git update successful!\n");
}

void storeAndLogData() {
    time_t last_github_update = 0;
    while (1) {
        // save current data to history
        time_t now = time(NULL);
        data.timestamp = now;
        history[currentIndex] = data;

        // log data to files
        logDataToFile();

        // update git every hour
        if (now - last_github_update >= 3600) {
            update_github();
            last_github_update = now;
        }

        // move to next index
        currentIndex = (currentIndex + 1) % DATA_POINTS;

        // reset current data
        data.keypresses = 0;
        data.mouse_moves = 0;
        data.left_clicks = 0;
        data.right_clicks = 0;
        data.middle_clicks = 0;

        sleep(INTERVAL);
    }
}

int main(int argc, char *argv[]) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("current working directory: %s\n", cwd);
    } else {
        perror("getcwd() error");
    }
    // set-up event tap
    CGEventMask eventMask = (1 << kCGEventKeyDown) | (1 << kCGEventLeftMouseDown) |
                            (1 << kCGEventRightMouseDown) | (1 << kCGEventMouseMoved) |
                            (1 << kCGEventOtherMouseDown) | (1 << kCGEventKeyUp) |
                            (1 << kCGEventLeftMouseUp) | (1 << kCGEventRightMouseUp) | 
                            (1 << kCGEventScrollWheel);
    
    CFMachPortRef eventTap = CGEventTapCreate(kCGAnnotatedSessionEventTap,
                                              kCGHeadInsertEventTap,
                                              0,
                                              eventMask,
                                              eventCallback,
                                              NULL);

    if (!eventTap) {
        fprintf(stderr, "Failed to create event tap!\n");
        return 1;
    }

    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);

    printf("event tap created and enabled!\n");

    // start the data storage and logging loop in a separate thread
    pthread_t logging_thread;
    pthread_create(&logging_thread, NULL, (void *)storeAndLogData, NULL);

    // start the event loop
    CFRunLoopRun();  // this keeps event tap running

    // cleanup (never reached unless CFRunLoopRun stops)
    CFRelease(eventTap);
    CFRelease(runLoopSource);

    return 0;
}