//
//  seeStatApp.swift
//  seeStat
//
//  Created by Bilal Siddiqui on 5/4/25.
//

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
    let seeDir = ProcessInfo.processInfo.environment["HOME"]! + "/code/see"
    var statusUpdateTimer: Timer?
    
    func applicationDidFinishLaunching(_ notification: Notification) {
        // Request notification permission
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound]) { granted, error in
            if let error = error {
                print("Notification permission error: \(error)")
            }
        }
        
        setupStatusItem()
    }
    
    func setupStatusItem() {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        
        if let button = statusItem?.button {
            button.title = "🧿"
            button.action = #selector(statusItemClicked(_:))
            button.sendAction(on: [.leftMouseUp, .rightMouseUp])
        }
        
        // start status update timer
        statusUpdateTimer = Timer.scheduledTimer(withTimeInterval: 5.0, repeats: true) { [weak self] _ in
            self?.updateStatusIndicator()
        }
        
        // initial update
        updateStatusIndicator()
    }
    
    @objc func statusItemClicked(_ sender: NSStatusBarButton) {
        let event = NSApp.currentEvent!
        
        if event.type == .rightMouseUp {
            showMenu()
        } else {
            showMenu()
        }
    }
    
    func showMenu() {
        let menu = NSMenu()
        
        // check status
        let isRunning = checkSeeStatus()
        let statusMenuItem = NSMenuItem(title: isRunning ? "see is running" : "see is stopped", action: nil, keyEquivalent: "")
        statusMenuItem.isEnabled = false
        menu.addItem(statusMenuItem)
        
        menu.addItem(NSMenuItem.separator())
        
        // toggle
        let toggleMenuItem = NSMenuItem(title: isRunning ? "stop see" : "start see", action: #selector(toggleSee(_:)), keyEquivalent: "")
        menu.addItem(toggleMenuItem)
        
        // view logs
        let viewLogsMenuItem = NSMenuItem(title: "view logs", action: #selector(viewLogs(_:)), keyEquivalent: "")
        menu.addItem(viewLogsMenuItem)
        
        // open webpage
        let openWebMenuItem = NSMenuItem(title: "open activity monitor", action: #selector(openWebpage(_:)), keyEquivalent: "")
        menu.addItem(openWebMenuItem)
        
        menu.addItem(NSMenuItem.separator())
        
        // quit
        let quitMenuItem = NSMenuItem(title: "quit", action: #selector(quitApp(_:)), keyEquivalent: "q")
        menu.addItem(quitMenuItem)
        
        statusItem?.menu = menu
        statusItem?.button?.performClick(nil)
        statusItem?.menu = nil
    }
    
    func checkSeeStatus() -> Bool {
        let pidFile = seeDir + "/.see.pid"
        
        do {
            let pidContent = try String(contentsOfFile: pidFile, encoding: .utf8)
            let pid = pidContent.trimmingCharacters(in: .whitespacesAndNewlines)
            
            // check if running
            let task = Process()
            task.launchPath = "/bin/ps"
            task.arguments = ["-p", pid]
            
            let pipe = Pipe()
            task.standardOutput = pipe
            task.standardError = pipe
            
            task.launch()
            task.waitUntilExit()
            
            return task.terminationStatus == 0
        } catch {
            return false
        }
    }
    
    @objc func toggleSee(_ sender: NSMenuItem) {
        if checkSeeStatus() {  // "if" was missing a space
            stopSee()
        } else {
            startSee()
        }
    }
    
    func startSee() {
        let task = Process()
        task.launchPath = "/bin/bash"
        task.arguments = [seeDir + "/launch.sh"]
        task.currentDirectoryPath = seeDir
        
        do {
            try task.run()
            task.waitUntilExit()
            
            // show notif
            showNotification(title: "see started", message: "activity monitor is now running")
        } catch {
            showNotification(title: "Error", message: "failed to start see: \(error.localizedDescription)")
        }
    }
    
    func stopSee() {
        let task = Process()
        task.launchPath = "/bin/bash"
        task.arguments = [seeDir + "/stop.sh"] 
        task.currentDirectoryPath = seeDir
        
        do {
            try task.run()
            task.waitUntilExit()
            showNotification(title: "See Stopped", message: "Activity monitor has been stopped")
        } catch {
            showNotification(title: "Error", message: "Failed to stop see: \(error.localizedDescription)")
        }
    }
    
    @objc func viewLogs(_ sender: NSMenuItem) {
        let logFile = seeDir + "/see.log"
        NSWorkspace.shared.open(URL(fileURLWithPath: logFile))
    }
    
    @objc func openWebpage(_ sender: NSMenuItem) {
        if let url = URL(string: "http://localhost:8000") {
            NSWorkspace.shared.open(url)
        }
    }
    
    @objc func quitApp(_ sender: NSMenuItem) {
        NSApplication.shared.terminate(nil)
    }
    
    func showNotification(title: String, message: String) {
        let notification = UNMutableNotificationContent()
        notification.title = title
        notification.body = message
        
        let request = UNNotificationRequest(identifier: UUID().uuidString, content: notification, trigger: nil)
        UNUserNotificationCenter.current().add(request)
    }
    
    func updateStatusIndicator() {
        guard let button = statusItem?.button else { return }
        let isRunning = checkSeeStatus()
        
        // update icon color or add badge
        if isRunning {
            button.title = "🧿"
        } else {
            button.title = "😑"
        }
    }
}
