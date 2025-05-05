import SwiftUI
import UserNotifications

@main
struct seeStatApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    
    var body: some Scene {
        Settings {
            EmptyView()
        }
    }
}

class AppDelegate: NSObject, NSApplicationDelegate {
    var statusItem: NSStatusItem?
    var statusUpdateTimer: Timer?
    
    // Use NSHomeDirectory() for sandbox safety
    let homePath = NSHomeDirectory()
    var seeDir: String {
        return "\(homePath)/code/see"
    }
    
    func applicationDidFinishLaunching(_ notification: Notification) {
        // Request notification permission - with error handling
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound]) { granted, error in
            if let error = error {
                print("Notification permission error: \(error)")
            } else {
                print("Notification permission granted: \(granted)")
            }
        }
        
        setupStatusItem()
    }
    
    // Rest of your code...
    
    // Modified process checking approach
    func checkSeeStatus() -> Bool {
        // Use a simple approach that doesn't require file access
        let task = Process()
        task.launchPath = "/usr/bin/pgrep"
        task.arguments = ["-f", "see/see"]
        
        let pipe = Pipe()
        task.standardOutput = pipe
        
        do {
            try task.run()
            let data = pipe.fileHandleForReading.readDataToEndOfFile()
            task.waitUntilExit()
            
            if let output = String(data: data, encoding: .utf8), !output.isEmpty {
                return true
            }
            return false
        } catch {
            print("Error checking process: \(error)")
            return false
        }
    }
    
    // Use AppleScript for operations that require more privileges
    func startSee() {
        let script = """
        do shell script "cd '\(seeDir)' && ./launch.sh" with administrator privileges
        """
        
        let appleScript = NSAppleScript(source: script)
        var error: NSDictionary?
        appleScript?.executeAndReturnError(&error)
        
        if let error = error {
            print("Error starting see: \(error)")
        } else {
            print("See started successfully")
        }
    }
    
    func stopSee() {
        let script = """
        do shell script "cd '\(seeDir)' && ./stop.sh" with administrator privileges
        """
        
        let appleScript = NSAppleScript(source: script)
        var error: NSDictionary?
        appleScript?.executeAndReturnError(&error)
        
        if let error = error {
            print("Error stopping see: \(error)")
        } else {
            print("See stopped successfully")
        }
    }
    
    // Rest of your methods...
}