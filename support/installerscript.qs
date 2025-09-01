// support/installerscript.qs  — CONTROLLER script
function Controller() {
    // Stable identity across versions
    installer.setValue("ProductUUID", "com.gollahalli.gav");

    // We plan to remove the dir ourselves; let non-empty dirs pass
    installer.setValue("AllowNonEmptyTargetDirectory", true);

    // Auto-answer the usual overwrite prompt if it appears
    installer.setMessageBoxAutomaticAnswer("OverwriteTargetDirectory", QMessageBox.Yes);

    console.log("gav: Controller loaded");
}

// ---------- helpers ----------
function lastSegment(p) {
    var s = String(p).replace(/\\+/g, "/");
    if (s.endsWith("/")) s = s.slice(0, -1);
    return s.substring(s.lastIndexOf("/") + 1);
}

// Only allow deleting dirs that clearly look like the GAV install dir
function isSafeInstallDir(dir) {
    if (!dir) return false;
    var seg = lastSegment(dir).toLowerCase();
    if (seg !== "gav") return false;               // guard: must end with /GAV or \GAV
    if (dir === "/" || /^[A-Za-z]:[\\\/]?$/.test(dir)) return false; // never root
    return true;
}

function rmrf(dir, os, elevateIfNeeded) {
    if (!isSafeInstallDir(dir)) {
        console.log("gav: REFUSING to delete non-GAV dir:", dir);
        return;
    }
    if (os === "x11" || os === "mac") {
        // Elevate if we’re in a system location (e.g. /opt, /usr)
        var needsRoot = dir.indexOf("/opt/") === 0 || dir.indexOf("/usr/") === 0 || dir === "/opt/GAV" || dir === "/usr/GAV";
        if (elevateIfNeeded && needsRoot && !installer.hasAdminRights())
            installer.gainAdminRights();
        var quoted = dir.replace(/'/g, "'\\''");
        var rc = installer.execute("bash", ["-c", "rm -rf -- '" + quoted + "'"]);
        console.log("gav: rm -rf rc =", rc);
    } else if (os === "win") {
        var win = dir.replace(/\//g, "\\");
        var rcw = installer.execute("cmd", ["/c", "rmdir", "/s", "/q", win]);
        console.log("gav: rmdir rc =", rcw);
    }
}

// If an existing IFW install is there, try a proper purge first; then rm -rf as fallback.
function purgeOrDelete(dir, os) {
    if (!dir) return;
    var mt = dir + "/maintenancetool" + (os === "win" ? ".exe" : "");
    if (installer.fileExists(mt)) {
        var rc = installer.execute(mt, ["purge", "-c"]); // older IFW: ["remove","-c"]
        console.log("gav: maintenancetool purge rc =", rc);
    }
    rmrf(dir, os, /*elevateIfNeeded=*/true);
}

// ---------- page hooks ----------
Controller.prototype.TargetDirectoryPageCallback = function () {
    var page = gui.currentPageWidget();  // TargetDirectoryPage
    var os   = installer.value("os");
    var dir  = page && page.targetDir ? page.targetDir() : installer.value("TargetDir");

    console.log("gav: TargetDirectoryPage dir =", dir);
    purgeOrDelete(dir, os);

    // Re-apply so the page re-validates after cleanup
    if (page && page.setTargetDir) page.setTargetDir(dir);
};

Controller.prototype.ReadyForInstallationPageCallback = function () {
    var dir = installer.value("TargetDir");
    var os  = installer.value("os");
    console.log("gav: ReadyForInstallation, ensuring dir is clean:", dir);
    purgeOrDelete(dir, os);
};

Controller.prototype.installationFinished = function () {
    var dir = installer.value("TargetDir");
    var os  = installer.value("os");
    var isAdmin = installer.hasAdminRights();
    var target  = dir + "/bin/gav";

    if (os === "win") {
        var bin = installer.toNativeSeparators(dir + "/bin");
        var exe = "@TargetDir@/bin/gav.exe";
        var ico = "@TargetDir@/logo.ico";

        // PATH (user) with undo
        var addPS = "$b='"+bin+"';$p=[Environment]::GetEnvironmentVariable('Path','User');" +
                    "if(-not($p.Split(';') -contains $b)){[Environment]::SetEnvironmentVariable('Path',$p+';'+$b,'User')}";
        var rmPS  = "$b='"+bin+"';$p=[Environment]::GetEnvironmentVariable('Path','User');" +
                    "$n=($p.Split(';')|?{$_-ne$b})-join(';');[Environment]::SetEnvironmentVariable('Path',$n,'User')";
        installer.performOperation("Execute", ["powershell.exe","-ExecutionPolicy","Bypass","-Command", addPS]);
        // no automatic undo stack in controller; uninstall will still remove files

        // Start menu shortcut
        installer.performOperation("CreateShortcut", [exe, "@StartMenuDir@/GAV.lnk",
            "description=GAV - A simple audio and video player",
            "iconPath="+ico, "iconId=0"]);
        return;
    }

    if (os === "x11") {
        var userBinDir = installer.value("HomeDir") + "/.local/bin";
        var sysBinDir  = "/usr/local/bin";
        var link       = (isAdmin ? sysBinDir : userBinDir) + "/gav";

        // system-wide symlink via ln -sfn when elevated; else per-user CreateLink
        if (isAdmin) {
            installer.performOperation("Execute", ["bash","-c","mkdir -p '"+sysBinDir.replace(/'/g,"'\\''")+"'"]);
            var t = target.replace(/'/g,"'\\''"), l = link.replace(/'/g,"'\\''");
            installer.performOperation("Execute", ["bash","-c","ln -sfn '"+t+"' '"+l+"'"]);
        } else {
            installer.performOperation("Mkdir", [userBinDir]);
            installer.performOperation("Delete", [link, "UNDOOPERATION", ""]);
            installer.performOperation("CreateLink", [link, target]);
        }

        // Desktop entry + icon
        var appsDir  = isAdmin ? "/usr/share/applications"
                               : installer.value("HomeDir") + "/.local/share/applications";
        var iconsDir = isAdmin ? "/usr/share/icons/hicolor/256x256/apps"
                               : installer.value("HomeDir") + "/.local/share/icons/hicolor/256x256/apps";
        installer.performOperation("Mkdir", [appsDir]);
        installer.performOperation("Mkdir", [iconsDir]);
        installer.performOperation("Copy", [dir + "/share/icons/hicolor/256x256/apps/icon.png",   iconsDir + "/gav.png"]);
        installer.performOperation("Copy", [dir + "/share/applications/gav.desktop", appsDir  + "/gav.desktop"]);
        installer.performOperation("Execute", ["update-desktop-database", appsDir, "ignoreExitCode=true"]);
        return;
    }

    if (os === "mac") {
        var linkDir = installer.value("HomeDir") + "/.local/bin";
        var linkMac = linkDir + "/gav";
        installer.performOperation("Mkdir", [linkDir]);
        installer.performOperation("Delete", [linkMac, "UNDOOPERATION", ""]);
        installer.performOperation("CreateLink", [linkMac, target]);
    }
};
