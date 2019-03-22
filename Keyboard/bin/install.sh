#!/bin/bash

if [ -e "/Applications/Preferences/Prefs-Keyboard" ]
then
	echo "Replacing keyboard preferences..."
	cp -f ./Keyboard /Applications/Preferences/Prefs-Keyboard
fi

if [ -e "/Applications/Preferences/Keyboard" ]
then
	echo "Replacing Dock's keyboard preferences..."
	cp -f ./Keyboard /Applications/Preferences/Keyboard
fi

echo
echo "Please read README file before using a program."
echo "Done!"
