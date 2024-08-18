compile with `gcc`:

```bash
gcc -o see see.c -framework ApplicationServices -lpthread -lm
```

run `see` as sudo:
```bash
sudo ./see
```

run in background using `nohup`:
```bash
nohup ./see &
```

this automatically saves `stdout` and `stderr` to a file `nohup.out`
you can change the file to something else:

```bash
nohup ./see > output.log 2>&1 &
```

check to see if it's running:
```bash
ps -ef | grep see
```

kill process: `kill <PID>` or `kill -9 <PID>`

debugging with `lldb`

```bash
lldb ./see
run
```

try to modify event tap location

```c
CFMachPortRef eventTap = CGEventTapCreate(kCGHIDEventTap,
                                          kCGHeadInsertEventTap,
                                          0,
                                          eventMask,
                                          eventCallback,
                                          NULL);
```

run as sudo:

```bash
sudo ./see
```

check for existing event taps

```bash
ioreg -l -w 0 | grep IOHIDEventSystem
```

remove `CGEventTapIsEnabled` -> fixes seg faults

events not being recorded?

check event mask

change `CGEventTapCreate`:

```c
CFMachPortRef eventTap = CGEventTapCreate(kCGAnnotatedSessionEventTap, kCGHeadInsertEventTap, 0, eventMask, eventCallback, NULL);
```

check event tap status

```c
if (!CGEventTapIsEnabled(eventTap)) {
    CGEventTapEnable(eventTap, true);
    printf("Re-enabled the event tap.\n");
}
```

test program:

```c
#include <ApplicationServices/ApplicationServices.h>
#include <stdio.h>

CGEventRef eventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    printf("Event detected! Type: %d\n", type);
    return event;
}

int main() {
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
        fprintf(stderr, "Failed to create event tap!\n");
        return 1;
    }

    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);

    printf("Event tap created and enabled.\n");

    CFRunLoopRun();  // Run the loop to keep capturing events

    CFRelease(eventTap);
    CFRelease(runLoopSource);

    return 0;
}
```

if test program works, it means there might be event/run loop handling issues and/or threading conflicts.

}

```