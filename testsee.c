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