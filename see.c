#include <ApplicationServices/ApplicationServices.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>
#include <stdbool.h>

#define DATA_INTERVAL 300    // 5 minutes in seconds
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
unsigned int consecutive_zero_intervals = 0;

void logPermissionStatus() {
    bool accessibility_trusted = AXIsProcessTrusted();
    printf("accessibility permission: %s\n", accessibility_trusted ? "granted" : "missing");
    if (!accessibility_trusted) {
        printf("grant in System Settings > Privacy & Security > Accessibility\n");
    }

    bool input_monitoring_trusted = CGPreflightListenEventAccess();
    printf("input monitoring permission: %s\n", input_monitoring_trusted ? "granted" : "missing");
    if (!input_monitoring_trusted) {
        printf("grant in System Settings > Privacy & Security > Input Monitoring\n");
        printf("you may need to restart `see` after granting permissions\n");
    }
}

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

// load cumulative data from file
void loadCumulativeData() {
	FILE *file = fopen("cumulative_data.csv", "r");
	if (!file) {
		printf("no previous cumulative data found, starting fresh\n");
		return;
	}

	char line[256];
	// skip header
	if (fgets(line, sizeof(line), file) != NULL) {
		// read data line
		if (fgets(line, sizeof(line), file) != NULL) {
			sscanf(line, "%llu,%lf,%llu,%llu,%llu",
					&cumulative_keypresses,
					&cumulative_mouse_moves,
					&cumulative_left_clicks,
					&cumulative_right_clicks,
					&cumulative_middle_clicks);
			printf("loaded previous cumulative data: %llu keypresses, %.2f mouse moves\n", cumulative_keypresses, cumulative_mouse_moves);
		}
	}
	fclose(file);
}

// load past 24h data from file
void loadPast24HoursData() {
	FILE *file = fopen("past_24_hours_data.csv", "r");
	if (!file) {
		printf("no previous 24h data found, starting fresh\n");
		return;
	}

	char line[256];
	int entryCount = 0;

	// skip header
	if (fgets(line, sizeof(line), file) != NULL) {
		time_t now = time(NULL);
		time_t cutoff = now - (24 * 3600);
		while (fgets(line, sizeof(line), file) != NULL && entryCount < DATA_POINTS) {
			if (strncmp(line, "cumulative,", 11) == 0) {
				continue;
			}
			time_t timestamp;
			int keypresses, left_clicks, right_clicks, middle_clicks;
			double mouse_moves;

			if (sscanf(line, "%ld,%d,%lf,%d,%d,%d",
						&timestamp, &keypresses, &mouse_moves, &left_clicks, &right_clicks, &middle_clicks) == 6) {
				if (timestamp >= cutoff) {
					history[entryCount].timestamp = timestamp;
					history[entryCount].keypresses = keypresses;
					history[entryCount].mouse_moves = mouse_moves;
					history[entryCount].left_clicks = left_clicks;
					history[entryCount].right_clicks = right_clicks;
					history[entryCount].middle_clicks = middle_clicks;
					entryCount++;
				}
			}
		}
		if (entryCount > 0) {
			currentIndex = entryCount % DATA_POINTS;
			printf("loaded %d previous data points from the last 24h\n", entryCount);
		}
	}
	fclose(file);
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
        int interval_keypresses = data.keypresses;
        double interval_mouse_moves = data.mouse_moves;
        int interval_left_clicks = data.left_clicks;
        int interval_right_clicks = data.right_clicks;
        int interval_middle_clicks = data.middle_clicks;

        printf("interval activity - keys: %d, mouse: %.4f m, clicks(L/R/M): %d/%d/%d\n",
               interval_keypresses,
               interval_mouse_moves,
               interval_left_clicks,
               interval_right_clicks,
               interval_middle_clicks);
        
        // store current minute's data
        history[currentIndex].timestamp = now;
        history[currentIndex].keypresses = interval_keypresses;
        history[currentIndex].mouse_moves = interval_mouse_moves;
        history[currentIndex].left_clicks = interval_left_clicks;
        history[currentIndex].right_clicks = interval_right_clicks;
        history[currentIndex].middle_clicks = interval_middle_clicks;

        if (interval_keypresses == 0 &&
            interval_mouse_moves == 0.0 &&
            interval_left_clicks == 0 &&
            interval_right_clicks == 0 &&
            interval_middle_clicks == 0) {
            consecutive_zero_intervals++;
            if (consecutive_zero_intervals % 12 == 0) {
                printf("warning: no activity events captured for %u minutes; check Accessibility and Input Monitoring permissions\n",
                       consecutive_zero_intervals * (DATA_INTERVAL / 60));
            }
        } else {
            consecutive_zero_intervals = 0;
        }

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
    logPermissionStatus();

    loadCumulativeData();
    loadPast24HoursData();
    
    // set up event tap
    CGEventMask eventMask = (1 << kCGEventKeyDown) | (1 << kCGEventLeftMouseDown) |
                           (1 << kCGEventRightMouseDown) | (1 << kCGEventMouseMoved) |
                           (1 << kCGEventOtherMouseDown);
    
    CFMachPortRef eventTap = CGEventTapCreate(kCGSessionEventTap,
                                             kCGHeadInsertEventTap,
                                             kCGEventTapOptionListenOnly,
                                             eventMask,
                                             eventCallback,
                                             NULL);

    if (!eventTap) {
        fprintf(stderr, "error: failed to create event tap\n");
        fprintf(stderr, "ensure Terminal (or your launcher) has Accessibility and Input Monitoring permissions\n");
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
