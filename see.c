#include <ApplicationServices/ApplicationServices.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>

#define DATA_INTERVAL 900    // 15 minutes in seconds
#define HOURS_TO_SECONDS(x) ((x) * 3600)
#define DATA_POINTS (HOURS_TO_SECONDS(24) / DATA_INTERVAL)  // 1440 points for 24h
#define PIXELS_TO_METERS(x) ((x) * 0.0002645833)

// files
const char *files[] = {"cumulative_data.csv", "past_24_hours_data.csv"};

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
        fprintf(cumulativeFile, "keypresses,mousemoves,leftclicks,rightclicks,middleclicks\n");
        fprintf(cumulativeFile, "%llu,%.2f,%llu,%llu,%llu\n",
                cumulative_keypresses,
                cumulative_mouse_moves,
                cumulative_left_clicks,
                cumulative_right_clicks,
                cumulative_middle_clicks);
        fclose(cumulativeFile);
    }

    // log past 24 hours
    FILE *past24HoursFile = fopen("past_24_hours_data.csv", "w");
    if (past24HoursFile) {
        fprintf(past24HoursFile, "timestamp,keypresses,mousemoves,leftclicks,rightclicks,middleclicks\n");
        
        time_t now = time(NULL);
        unsigned long long past24_keypresses = 0;
        double past24_mouse_moves = 0.0;
        unsigned long long past24_left_clicks = 0;
        unsigned long long past24_right_clicks = 0;
        unsigned long long past24_middle_clicks = 0;

        for (int i = 0; i < DATA_POINTS; i++) {
            if (history[i].timestamp > now - (24 * 3600)) {  // Only last 24h
                fprintf(past24HoursFile, "%ld,%d,%.2f,%d,%d,%d\n",
                        history[i].timestamp,
                        history[i].keypresses,
                        history[i].mouse_moves,
                        history[i].left_clicks,
                        history[i].right_clicks,
                        history[i].middle_clicks);

                past24_keypresses += history[i].keypresses;
                past24_mouse_moves += history[i].mouse_moves;
                past24_left_clicks += history[i].left_clicks;
                past24_right_clicks += history[i].right_clicks;
                past24_middle_clicks += history[i].middle_clicks;
            }
        }

        // add cumulative line for last 24h
        fprintf(past24HoursFile, "cumulative,%llu,%.2f,%llu,%llu,%llu\n",
                past24_keypresses, past24_mouse_moves,
                past24_left_clicks, past24_right_clicks, past24_middle_clicks);
        
        fclose(past24HoursFile);
        printf("Updated past 24 hours data with %d entries\n", currentIndex);
    }
}

// new helper funcs
// also updated to capture HTTP responses
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    // if userp is NULL, discard response
    if (!userp) {
        return size * nmemb;
    }

    // store response in buf
    size_t total_size = size * nmemb;
    char *buffer = (char *)userp;

    // copy response if buffer is large enough
    strncpy(buffer, contents, total_size);
    buffer[total_size] = '\0'; // null termination

    return total_size;
}

// timestamp tracking for gh events
void *timerThread(void *arg) {
    while (1) {
        sleep(DATA_INTERVAL);  // wait 1 minute
        
        time_t now = time(NULL);
        
        // store current minute's data
        history[currentIndex].timestamp = now;
        history[currentIndex].keypresses = data.keypresses;
        history[currentIndex].mouse_moves = data.mouse_moves;
        history[currentIndex].left_clicks = data.left_clicks;
        history[currentIndex].right_clicks = data.right_clicks;
        history[currentIndex].middle_clicks = data.middle_clicks;

        // reset current interval data
        data = (ActivityData){0};
        data.timestamp = now;

        // update index
        currentIndex = (currentIndex + 1) % DATA_POINTS;

        // always log data to files
        logDataToFile();
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // forcing line buffering for stdout
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    printf("starting see...\n");
    fflush(stdout); // explicit flush
    
    // set up event tap
    CGEventMask eventMask = (1 << kCGEventKeyDown) | (1 << kCGEventLeftMouseDown) |
                           (1 << kCGEventRightMouseDown) | (1 << kCGEventMouseMoved) |
                           (1 << kCGEventOtherMouseDown);
    
    CFMachPortRef eventTap = CGEventTapCreate(kCGAnnotatedSessionEventTap,
                                             kCGHeadInsertEventTap,
                                             0,
                                             eventMask,
                                             eventCallback,
                                             NULL);

    if (!eventTap) {
        return 1;
    }

    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);

    pthread_t timer_thread;
    if (pthread_create(&timer_thread, NULL, timerThread, NULL) != 0) {
        return 1;
    }

    CFRunLoopRun();

    return 0;
}