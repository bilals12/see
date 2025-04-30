#include <ApplicationServices/ApplicationServices.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>
#include <errno.h>

#define DATA_INTERVAL 60    // 1 minute in seconds
#define GITHUB_INTERVAL 86000  // 1 hour in seconds
#define HOURS_TO_SECONDS(x) ((x) * 3600)
#define DATA_POINTS (HOURS_TO_SECONDS(24) / DATA_INTERVAL)  // 1440 points for 24h
#define PIXELS_TO_METERS(x) ((x) * 0.0002645833)

// files
const char *files[] = {"cumulative_data.csv", "past_24_hours_data.csv"};

// gh configs to be loaded from .env
char *github_token = NULL;
char *github_repo = NULL;

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

        // store GitHub specific values
        if (strcmp(key, "GITHUB_TOKEN") == 0) {
            github_token = strdup(value);
        } else if (strcmp(key, "GITHUB_REPO") == 0) {
            github_repo = strdup(value);
        }
        
        setenv(key, value, 1);
    }

    fclose(env_file);
    
    // validate GitHub configuration
    if (!github_token || !github_repo) {
        fprintf(stderr, "Missing required GitHub configuration in .env\n");
    }
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

// updating git
void update_github() {
    time_t now = time(NULL);
    char timestamp[26];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    printf("attempting to update github @ %s...\n", timestamp);

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "curl initialization failed\n");
        return;
    }

    // validate token
    if (!github_token) {
        fprintf(stderr, "GITHUB_TOKEN not set\n");
        curl_easy_cleanup(curl);
        return;
    }

    if (!github_repo) {
        github_repo = "bilals12/see";
        printf("using default repo: %s\n", github_repo);
    }

    // set up headers correctly
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "User-Agent: see-activity-tracker/1.0");
    headers = curl_slist_append(headers, "X-GitHub-Api-Version: 2022-11-28");
    
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", github_token);
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "see-activity-tracker/1.0");

    for (int i = 0; i < 2; i++) {
        // get current SHA if file exists
        char check_url[256];
        snprintf(check_url, sizeof(check_url), 
                "https://api.github.com/repos/%s/contents/%s",
                github_repo, files[i]);

        char response[8192] = {0};
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
        curl_easy_setopt(curl, CURLOPT_URL, check_url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

        printf("checking if file exists: %s\n", check_url);
        CURLcode res = curl_easy_perform(curl);
        char sha[64] = {0};
        int file_exists = 0;
        if (res == CURLE_OK) {
            long status_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

            if (status_code == 200) {
                file_exists = 1;
                char *sha_start = strstr(response, "\"sha\":\"");
                if (sha_start) {
                    sha_start += 7; // move past the "\"sha\":\""
                    char *sha_end = strchr(sha_start, '"');
                    if (sha_end) {
                        strncpy(sha, sha_start, sha_end - sha_start);
                        printf("file exists with SHA: %s\n", sha);
                    }
                }
            } else {
                printf("file doesn't exist (status code: %ld)\n", status_code);
            }
        } else {
            fprintf(stderr, "failed to check file existence: %s\n", curl_easy_strerror(res));
        }

        // get file content and base64 encode it
        FILE *fp = fopen(files[i], "rb");
        if (!fp) {
            fprintf(stderr, "failed to open file %s: %s\n", files[i], strerror(errno));
            continue;
        }

        fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        char *content = malloc(fsize + 1);
        if (!content) {
            fprintf(stderr, "memory allocation failed\n");
            fclose(fp);
            continue;
        }

        fread(content, 1, fsize, fp);
        content[fsize] = '\0';
        fclose(fp);
        
        // create tmp file
        // use base64 command properly
        char tmp_file[256];
        snprintf(tmp_file, sizeof(tmp_file), "/tmp/see_content_%d.txt", i);
        FILE *tmp = fopen(tmp_file, "wb");
        if (!tmp) {
            fprintf(stderr, "failed to create temp file: %s\n", strerror(errno));
            continue;
        }
        fwrite(content, 1, fsize, tmp);
        fclose(tmp);

        // macOS compatible b64
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "cat %s | base64", tmp_file);
        printf("running %s\n", cmd);

        FILE *b64 = popen(cmd, "r");
        if (!b64) {
            fprintf(stderr, "failed to run base64: %s\n", strerror(errno));
            unlink(tmp_file);
            free(content);
            continue;
        }
        char *base64_content = malloc(fsize * 2);
        if (!base64_content) {
            fprintf(stderr, "memory allocation failed to run for b64 content\n");
            pclose(b64);
            unlink(tmp_file);
            free(content);
            continue;
        }

        size_t b64_size = fread(base64_content, 1, fsize * 2 - 1, b64);
        base64_content[b64_size] = '\0';
        pclose(b64);
        unlink(tmp_file);

        // remove newlines from base64 output
        for (char *p = base64_content; *p;  p++) {
            if (*p == '\n' || *p == '\r') {
                *p = '\0';
                break;
            }
        }
        printf("b64 encoding completed (length: %zu)\n", strlen(base64_content));

        // create proper JSON payload
        char *payload;
        if (file_exists && sha[0] != '\0') {
            asprintf(&payload, 
                    "{\"message\":\"Update %s - %s\","
                    "\"committer\":{\"name\":\"bilals12\",\"email\":\"bilal.s12@protonmail.com\"},"
                    "\"content\":\"%s\",\"sha\":\"%s\"}",
                    files[i], timestamp, base64_content, sha);
        } else {
            asprintf(&payload, 
                    "{\"message\":\"Create %s - %s\","
                    "\"committer\":{\"name\":\"bilals12\",\"email\":\"bilal.s12@protonmail.com\"},"
                    "\"content\":\"%s\"}",
                    files[i], timestamp, base64_content);
        }

        // send update
        curl_easy_setopt(curl, CURLOPT_URL, check_url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        
        memset(response, 0, sizeof(response));
        printf("sending update to github...\n");
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "failed to update %s: %s\n", files[i], curl_easy_strerror(res));
        } else {
            long status_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

            if (status_code >= 200 && status_code < 300) {
                printf("successfully updated %s (HTTP %ld)\n", files[i], status_code);
            } else {
                fprintf(stderr, "gh api error (HTTP %ld): %s\n", status_code, response);
            }
        }
        
        free(content);
        free(base64_content);
        free(payload);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    printf("github update completed!");
}

// timestamp tracking for gh events
void *timerThread(void *arg) {
    time_t last_github_update = time(NULL);
    
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

        // only update GitHub every hour
        if (now - last_github_update >= GITHUB_INTERVAL) {
            update_github();
            last_github_update = now;
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // forcing line buffering for stdout
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    printf("starting see...\n");
    fflush(stdout); // explicit flush
    
    load_env();
    
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