#!/bin/bash

echo "Welcome to FileExpander installation!"
echo "-----------------------------------------------------------------"
echo "Copyright (c) 2004 Ruslan Nickolaev (nruslan@hotbox.ru)"
echo "This program is released under the GNU General Public License 2.0"
echo "See doc/COPYING file for more detalies"
echo "-----------------------------------------------------------------"
read -p "Are you accept all the terms of GPL 2.0 license (y/n)? " -e ACCEPT_LICENSE

if [ $ACCEPT_LICENSE == "y" ]; then

    # removing old version
    echo "Removing previous version..."
    rm -f /etc/expander.rules
    rm -f /bin/Expander

    # installing new version
    echo "Installing FileExpander..."
    cp -f ./FileExpander /system/bin/FileExpander
    cp -L -f ./FileExpander.rules /etc/FileExpander.rules
    cp -f ./scripts/unpack-fe /usr/bin/unpack-fe

    # linking LBrowser
    if [ -e "/system/bin/FileBrowser" ]
    then
        echo "Found Dock installed or previously linked LBrowser."
        echo "Creating FileExpander menu item..."
        mkdir -p ~/Dock/Applications
        ln -sf /system/bin/FileExpander ~/Dock/Applications/FileExpander
    else
        echo "Dock is not installed. Linking LBrowser..."
        ln -s /Applications/Launcher/bin/LBrowser /system/bin/FileBrowser
    fi

    if [ -e "/system/registrar" ]
    then
        echo "Registrar server was found. You can register new file types associated with FileExpander..."
        read -p "Would you like do it now (y/n)? " REGISTER_TYPES
        if [ $REGISTER_TYPES == "y" ]; then
            cp -f ./types_icons/*.png /system/icons/filetypes
            ./ftreg "zip file" application/x-zip /system/bin/FileExpander /system/icons/filetypes/zip-file.png zip
            ./ftreg "tar.gz file" application/x-gtar /system/bin/FileExpander /system/icons/filetypes/tgz-file.png tgz tar.gz
            ./ftreg "tar.bz2 file" application/x-btar /system/bin/FileExpander /system/icons/filetypes/tbz-file.png tbz tar.bz2
            ./ftreg "tar.Z file" application/x-ztar /system/bin/FileExpander /system/icons/filetypes/Z-file.png tar.Z
            ./ftreg "tar file" application/x-tar /system/bin/FileExpander /system/icons/filetypes/tar-file.png tar
            ./ftreg "gzip file" application/x-gzip /system/bin/FileExpander /system/icons/filetypes/gzip-file.png gz
            ./ftreg "bzip2 file" application/x-bzip /system/bin/FileExpander /system/icons/filetypes/bzip2-file.png bz2
            ./ftreg "compress file" application/x-compress /system/bin/FileExpander /system/icons/filetypes/Z-file.png Z
        fi
    fi

    echo "Install finished!"
    echo
    echo "Syllable 0.5.3 note: please don't forget to update dlogin program (dlogin-0.5.3-update.tar.bz2 package); otherwise FileExpander will work incorrectly!"
fi
