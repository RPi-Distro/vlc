'''apport package hook for vlc

(c) 2010 Canonical Ltd.
Author: Brian Murray <brian@ubuntu.com>
'''

from apport.hookutils import attach_related_packages

def add_info(report):
	attach_related_packages(report, [
		"libaa1",
		"libgl1-mesa-glx",
		"libgl1",
		"libglib2.0-0",
		"libgtk2.0-0",
		"libnotify1",
		"libnotify1-gtk2.10",
		"libqtcore4",
		"libqtgui4",
		"libsdl-image1.2",
		"libsdl1.2debian",
		"libx11-6",
		"libx264-85",
		"libxcb-keysyms1",
		"libxcb1",
		"libxext6",
		"libxinerama1",
		"libxv1",
		"libxxf86vm1",
		] )
