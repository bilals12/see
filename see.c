#include <ApplicationServices/ApplicationServices.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>

#define DATA_INTERVAL 60    // 1 minute in seconds
#define GITHUB_INTERVAL 3600  // 1 hour in seconds
#define HOURS_TO_SECONDS(x) ((x) * 3600)
#define DATA_POINTS (HOURS_TO_SECONDS(24) / DATA_INTERVAL)  // 1440 points for 24h
#define PIXELS_TO_METERS(x) ((x) * 0.0002645833)

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

void load_env() {
    FILE *env_file = fopen(".env", "r");
        if (!env_file) {
            fprintf(stderr, ".env file not found\n");
            return;
        }

        char line[256];
        while (fgets(line, sizeof(line), env_file)) {
            char *newline = strchr(line, '\n');
            if (newline) *newline = 0;

            char *equals = strchr(line, '=');
            if (!equals) continue;

            *equals = 0;
            const char *key = line;
            const char *value = equals + 1;

            setenv(key, value, 1);
        }

        fclose(env_file);
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
        fprintf(past24HoursFile, "cumulative,%llu,%.2f meters,%llu,%llu,%llu\n",
                past24_keypresses, past24_mouse_moves,
                past24_left_clicks, past24_right_clicks, past24_middle_clicks);
        
        fclose(past24HoursFile);
        printf("Updated past 24 hours data with %d entries\n", currentIndex);
    }
}

// new helper funcs
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    return size * nmemb; // discard response
} 

// updating git
void update_github() {
    time_t now = time(NULL);
    char timestamp[26];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // printf("attempting to update github @ %s...\n", timestamp);

    CURL *curl;
    CURLcode res;

    // init curl
    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "curl initialization failed\n");
        return;
    }

    // read files into memory
    FILE *cumulative = fopen("cumulative_data.csv", "rb");
    FILE *past24 = fopen("past_24_hours_data.csv", "rb");

    if (!cumulative) {
        fprintf(stderr, "failed to open cumulative_data.csv: %s\n", strerror(errno));
        curl_easy_cleanup(curl);
        return;
    }

    if (!past24) {
        fprintf(stderr, "failed to open past_24_hours_data.csv: %s\n", strerror(errno));
        fclose(cumulative);
        curl_easy_cleanup(curl);
        return;
    }

    // gh token
    const char* github_token = getenv("GITHUB_TOKEN");
    if (!github_token) {
        fprintf(stderr, "GITHUB_TOKEN environment variable not set\n");
        fclose(cumulative);
        fclose(past24);
        curl_easy_cleanup(curl);
        return;
    }

    // gh repo from env
    const char* github_repo = getenv("GITHUB_REPO");
    if (!github_repo) {
        github_repo = "bilals12/see";  // fallback default
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", github_token);
    headers = curl_slist_append(headers, auth_header);

    // set common options
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    // update files with gh api
    const char *files[] = {"cumulative_data.csv", "past_24_hours_data.csv"};
    FILE *sources[] = {cumulative, past24};

    for (int i = 0; i < 2; i++) {
        // file content + size
        fseek(sources[i], 0, SEEK_END);
        long fsize = ftell(sources[i]);
        fseek(sources[i], 0, SEEK_SET);

        char *content = malloc(fsize + 1);
        fread(content, fsize, 1, sources[i]);
        content[fsize] = 0;

        // json payload
        char *payload;
        asprintf(&payload, "{\"message\":\"Update %s - %s\",\"content\":\"%s\"}", files[i], timestamp, content);

        // set url
        char url[256];
        snprintf(url, sizeof(url), 
                "https://api.github.com/repos/%s/contents/%s", 
                github_repo, files[i]);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "failed to update %s: %s\n", files[i], curl_easy_strerror(res));
        }

        free(content);
        free(payload);
    }
    
    // cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    fclose(cumulative);
    fclose(past24);

    printf("github update completed!\n");
}

// timestamp tracking for gh events
void *timerThread(void *arg) {
    time_t last_github_update = time(NULL);
    
    while (1) {
        sleep(DATA_INTERVAL);  // Wait 1 minute
        
        time_t now = time(NULL);
        
        // Store current minute's data
        history[currentIndex].timestamp = now;
        history[currentIndex].keypresses = data.keypresses;
        history[currentIndex].mouse_moves = data.mouse_moves;
        history[currentIndex].left_clicks = data.left_clicks;
        history[currentIndex].right_clicks = data.right_clicks;
        history[currentIndex].middle_clicks = data.middle_clicks;

        // Reset current interval data
        data = (ActivityData){0};
        data.timestamp = now;

        // Update index
        currentIndex = (currentIndex + 1) % DATA_POINTS;

        // Always log data to files
        logDataToFile();

        // Only update GitHub every hour
        if (now - last_github_update >= GITHUB_INTERVAL) {
            update_github();
            last_github_update = now;
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    load_env();
    
    // Set up event tap
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