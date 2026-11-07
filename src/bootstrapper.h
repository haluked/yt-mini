#ifndef BOOTSTRAPPER_H
#define BOOTSTRAPPER_H

#include <windows.h>

/* Gerekli yardımcı araçlar (yt-dlp, ffmpeg) sistemde veya app dizininde mevcut mu kontrol eder;
   eksikse ilk kurulum penceresi göstererek %LOCALAPPDATA%\ff-dlp-viewer\bin dizinine indirir.
   Kullanıcı iptal ederse veya hata olursa 0, araçlar hazırsa 1 döner. */
int bootstrapper_ensure_tools(HWND hParent);

/* yt-dlp'nin en son sürümünü GitHub üzerinden indirip günceller. */
int bootstrapper_update_ytdlp(HWND hParent);

#endif /* BOOTSTRAPPER_H */
