function Component()
{
    component.loaded.connect(this, Component.prototype.installerLoaded);
    installer.setDefaultPageVisible(QInstaller.TargetDirectory, false);
}

Component.prototype.installerLoaded = function()
{
}

Component.prototype.createOperations = function()
{
    component.createOperations();

    var target = installer.value("TargetDir") + "/bin/gav";

    if (installer.value("os") === "win") {
        var binDir = installer.value("TargetDir").replace(/\//g, '\\') + '\\bin';
        var addPathCommand = "$binDir='" + binDir + "'; $userPath=[Environment]::GetEnvironmentVariable('Path','User'); if(-not($userPath.Split(';') -contains $binDir)){$newPath=$userPath+';'+$binDir; [Environment]::SetEnvironmentVariable('Path',$newPath,'User')}";
        component.addOperation("Execute", "powershell.exe", "-ExecutionPolicy", "Bypass", "-Command", addPathCommand);

        var removePathCommand = "$binDir='" + binDir + "'; $userPath=[Environment]::GetEnvironmentVariable('Path','User'); $pathArray=$userPath.Split(';')|?{$_-ne$binDir}; $newPath=$pathArray-join(';'); [Environment]::SetEnvironmentVariable('Path',$newPath,'User')";
        component.addUndoOperation("Execute", "powershell.exe", "-ExecutionPolicy", "Bypass", "-Command", removePathCommand);

    } else if (installer.value("os") === "x11" || installer.value("os") === "mac") {
        var linkName = "";
        if (installer.isAdmin()) {
            linkName = "/usr/local/bin/gav";
        } else {
            var localBin = installer.value("HomeDir") + "/.local/bin";
            component.addOperation("Mkdir", localBin);
            linkName = localBin + "/gav";
        }
        component.addOperation("CreateLink", linkName, target, "force=true");
    }
}

Component.prototype.targetChanged = function (text)
{
    var widget = gui.currentPageWidget(); // get the current wizard page
    var install = false;

    if (widget != null)
    {
        if (text != "")
        {
            if (installer.fileExists(text + "/components.xml"))
            {
                var result = QMessageBox.question("quit.question", "Installer", "Do you want to overwrite previous installation?",
                    QMessageBox.Yes | QMessageBox.No);
                if (result == QMessageBox.Yes)
                {
                   install = true;
                }
            }
            else
                install = true;
        }
        else
            install = false;
    }

    widget.complete = install;

    if(install) {
        installer.setValue("TargetDir", text);
    }
}
