#ifndef BOOTSTRAPPER_H
#define BOOTSTRAPPER_H

#include <windows.h>

/* Checks whether the required tools (yt-dlp, ffmpeg) exist on the system or in the app folder;
   if missing, shows the first-run window and downloads them into %LOCALAPPDATA%\ff-dlp-viewer\bin.
   Returns 0 if the user cancels or an error occurs, 1 when the tools are ready. */
int bootstrapper_ensure_tools(HWND hParent);

/* Downloads and updates the latest yt-dlp release from GitHub. */
int bootstrapper_update_ytdlp(HWND hParent);

#endif /* BOOTSTRAPPER_H */
