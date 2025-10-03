// support/installerscript.qs — CONTROLLER script for GAV installer
function Controller() {
    // Stable identity across versions
    installer.setValue("ProductUUID", "com.gollahalli.gav");
    
    // Allow installation over existing directory
    installer.setValue("AllowNonEmptyTargetDirectory", "true");
    installer.setMessageBoxAutomaticAnswer("OverwriteTargetDirectory", QMessageBox.Yes);
    
    console.log("GAV: Controller initialized");
}

// ---------- Page Callbacks ----------

// Add License Agreement page
Controller.prototype.IntroductionPageCallback = function() {
    var widget = gui.currentPageWidget();
    if (widget != null) {
        widget.MessageLabel.setText("Welcome to the GAV installer");
    }
}

// Show license acceptance page
Controller.prototype.LicenseAgreementPageCallback = function() {
    var page = gui.currentPageWidget();
    if (page != null) {
        console.log("GAV: License page shown");
    }
}

// Clean up old installation before installing
Controller.prototype.TargetDirectoryPageCallback = function() {
    var targetDir = installer.value("TargetDir");
    console.log("GAV: Target directory:", targetDir);
    
    // Check if old installation exists
    cleanupOldInstallation(targetDir);
}

// Final cleanup check before installation
Controller.prototype.ReadyForInstallationPageCallback = function() {
    var targetDir = installer.value("TargetDir");
    console.log("GAV: Ready for installation to:", targetDir);
    cleanupOldInstallation(targetDir);
}

// Post-installation setup
Controller.prototype.FinishedPageCallback = function() {
    var targetDir = installer.value("TargetDir");
    var os = systemInfo.productType;
    
    console.log("GAV: Installation finished");
    console.log("GAV: Target directory:", targetDir);
    console.log("GAV: OS:", os);
    
    try {
        if (os === "windows") {
            setupWindows(targetDir);
        } else if (os === "osx") {
            setupMacOS(targetDir);
        } else {
            setupLinux(targetDir);
        }
    } catch (e) {
        console.log("GAV: Setup error:", e);
    }
}

// ---------- Helper Functions ----------

function cleanupOldInstallation(targetDir) {
    if (!targetDir || targetDir === "") return;
    
    // Check if maintenance tool exists
    var os = systemInfo.productType;
    var maintenanceTool = targetDir + "/gav_MaintenanceTool" + (os === "windows" ? ".exe" : "");
    
    if (installer.fileExists(maintenanceTool)) {
        console.log("GAV: Found existing installation, attempting cleanup");
        try {
            // Try to run uninstaller silently
            installer.execute(maintenanceTool, ["--uninstall"]);
        } catch (e) {
            console.log("GAV: Cleanup warning:", e);
        }
    }
}

function setupWindows(targetDir) {
    console.log("GAV: Setting up Windows environment");
    
    var binDir = targetDir + "\\bin";
    var exePath = binDir + "\\gav.exe";
    var iconPath = targetDir + "\\logo.ico";
    
    // Create Start Menu shortcut
    var startMenuDir = installer.value("StartMenuDir");
    if (startMenuDir && startMenuDir !== "") {
        try {
            var shortcutPath = startMenuDir + "\\GAV.lnk";
            component.addOperation("CreateShortcut",
                exePath,
                shortcutPath,
                "workingDirectory=" + binDir,
                "iconPath=" + iconPath,
                "iconId=0",
                "description=GAV - Audio and Video Player");
            console.log("GAV: Created Start Menu shortcut");
        } catch (e) {
            console.log("GAV: Start Menu shortcut error:", e);
        }
    }
    
    // Add to PATH (optional - user can do this manually if preferred)
    try {
        component.addOperation("EnvironmentVariable",
            "PATH",
            binDir,
            true,  // prepend
            "HKCU"); // user environment
        console.log("GAV: Added to PATH");
    } catch (e) {
        console.log("GAV: PATH setup error:", e);
    }
}

function setupMacOS(targetDir) {
    console.log("GAV: Setting up macOS environment");
    
    var homeDir = installer.value("HomeDir");
    var binDir = homeDir + "/.local/bin";
    var gavBinary = targetDir + "/bin/gav";
    var symlinkPath = binDir + "/gav";
    
    try {
        // Create .local/bin if it doesn't exist
        component.addOperation("Mkdir", binDir);
        
        // Create symlink
        component.addOperation("CreateLink", symlinkPath, gavBinary);
        
        console.log("GAV: Created symlink:", symlinkPath, "->", gavBinary);
    } catch (e) {
        console.log("GAV: macOS setup error:", e);
    }
}

function setupLinux(targetDir) {
    console.log("GAV: Setting up Linux environment");
    
    var homeDir = installer.value("HomeDir");
    var hasAdminRights = installer.gainAdminRights();
    
    var gavBinary = targetDir + "/bin/gav";
    var desktopFile = targetDir + "/share/applications/gav.desktop";
    var iconFile = targetDir + "/share/icons/hicolor/256x256/apps/gav.png";
    
    // Setup binary symlink
    try {
        if (hasAdminRights) {
            // System-wide installation
            component.addOperation("Mkdir", "/usr/local/bin");
            component.addOperation("CreateLink", "/usr/local/bin/gav", gavBinary);
            console.log("GAV: Created system-wide symlink");
        } else {
            // User installation
            var userBinDir = homeDir + "/.local/bin";
            component.addOperation("Mkdir", userBinDir);
            component.addOperation("CreateLink", userBinDir + "/gav", gavBinary);
            console.log("GAV: Created user symlink");
        }
    } catch (e) {
        console.log("GAV: Binary symlink error:", e);
    }
    
    // Setup desktop entry and icon
    try {
        var appsDir = hasAdminRights ? "/usr/share/applications" : homeDir + "/.local/share/applications";
        var iconsDir = hasAdminRights ? "/usr/share/icons/hicolor/256x256/apps" : homeDir + "/.local/share/icons/hicolor/256x256/apps";
        
        component.addOperation("Mkdir", appsDir);
        component.addOperation("Mkdir", iconsDir);
        
        if (installer.fileExists(desktopFile)) {
            component.addOperation("Copy", desktopFile, appsDir + "/gav.desktop");
            console.log("GAV: Installed desktop file");
        }
        
        if (installer.fileExists(iconFile)) {
            component.addOperation("Copy", iconFile, iconsDir + "/gav.png");
            console.log("GAV: Installed icon");
        }
        
        // Update desktop database
        component.addElevatedOperation("Execute", "update-desktop-database", appsDir);
    } catch (e) {
        console.log("GAV: Desktop integration error:", e);
    }
}
