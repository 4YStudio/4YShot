# Registers a user-local desktop entry so KWin authorizes the development
# binary to call the restricted org.kde.KWin.ScreenShot2 D-Bus interface,
# which the scrolling long-capture feature depends on.
#
# KWin grants that interface only to processes whose executable path matches
# the Exec key of a desktop file that declares the interface under
# X-KDE-DBUS-Restricted-Interfaces, so the entry must point at this exact
# build artifact path. NoDisplay keeps it out of the application menu while
# remaining visible to KWin's service trader.

if(NOT FOURYSHOT_EXE OR "$ENV{HOME}" STREQUAL "")
    return()
endif()

set(_desktop_dir "$ENV{HOME}/.local/share/applications")
file(MAKE_DIRECTORY "${_desktop_dir}")
file(WRITE "${_desktop_dir}/4yshot-dev.desktop"
"[Desktop Entry]
Type=Application
Name=4YShot (dev build)
Exec=${FOURYSHOT_EXE}
Terminal=false
NoDisplay=true
X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2
")

find_program(_kbuildsycoca NAMES kbuildsycoca6 kbuildsycoca)
if(_kbuildsycoca)
    execute_process(COMMAND "${_kbuildsycoca}" --noincremental RESULT_VARIABLE _result)
    if(NOT _result EQUAL 0)
        message(STATUS "kbuildsycoca exited with ${_result}; KWin may pick up the dev authorization after a short delay")
    endif()
endif()
