function Component()
{
    component.loaded.connect(this, Component.prototype.installerLoaded);
    installer.setDefaultPageVisible(QInstaller.TargetDirectory, false);
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
