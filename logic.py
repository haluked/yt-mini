import subprocess
import os
import shutil
import logging
import re
import tkinter as tk
from tkinter import messagebox
from config import add_to_history, CREATE_NO_WINDOW

current_process = None

def format_size(size_bytes):
    try:
        s = float(size_bytes)
        for unit in ['B', 'KB', 'MB', 'GB']:
            if s < 1024.0: return f"{s:.1f} {unit}"
            s /= 1024.0
        return f"{s:.1f} TB"
    except: return "N/A"

def install_via_winget(package_id):
    try:
        logging.info(f"Starting Winget install for {package_id}")
        subprocess.run(["start", "cmd", "/k", f"winget install {package_id} && exit"], shell=True)
    except Exception as e:
        logging.error(f"Winget Error: {e}")
        messagebox.showerror("Error", f"Could not launch installer: {e}")

def update_tools(yt_path):
    msg = "Update process started...\n\n"
    if os.path.exists(yt_path):
        try:
            subprocess.run([yt_path, "-U"], shell=False)
            msg += "✅ yt-dlp update command sent.\n"
            logging.info("yt-dlp update triggered")
        except Exception as e:
            logging.error(f"yt-dlp update failed: {e}")
            msg += "❌ yt-dlp update failed.\n"
    else:
        msg += "❌ yt-dlp not found.\n"
    
    subprocess.run(["start", "cmd", "/k", "winget upgrade Gyan.FFmpeg && exit"], shell=True)
    msg += "✅ FFmpeg update launched in new window."
    messagebox.showinfo("Updater", msg)

def auto_detect_ytdlp():
    path = shutil.which("yt-dlp")
    if not path:
        winget_path = os.path.expanduser(r"~\AppData\Local\Microsoft\WinGet\Packages")
        for root, dirs, files in os.walk(winget_path):
            if "yt-dlp.exe" in files: return os.path.join(root, "yt-dlp.exe")
    return path or ""

def get_all_ffmpeg_paths():
    found_paths = []
    try:
        result = subprocess.run(["where", "ffmpeg"], capture_output=True, text=True)
        if result.returncode == 0:
            found_paths.extend(result.stdout.strip().splitlines())
    except: pass
    common_locs = [r"C:\ffmpeg\bin\ffmpeg.exe", r"C:\Program Files\ffmpeg\bin\ffmpeg.exe"]
    for p in common_locs:
        if os.path.exists(p) and p not in found_paths: found_paths.append(p)
    return list(dict.fromkeys(found_paths))

# --- OPEN FUNCTIONS ---
def open_folder_safe(file_path):
    file_path = os.path.normpath(file_path.strip())
    if os.path.exists(file_path):
        subprocess.run(['explorer', '/select,', file_path])
    else:
        # Fallback: Open the folder if the file is gone
        folder = os.path.dirname(file_path)
        if os.path.exists(folder):
            os.startfile(folder)
        else:
            logging.warning(f"Failed to open path: {file_path}")
            messagebox.showerror("Error", f"Folder not found:\n{folder}")

def open_file_safe(file_path):
    file_path = os.path.normpath(file_path.strip())
    if os.path.exists(file_path):
        try:
            os.startfile(file_path)
        except Exception as e:
            messagebox.showerror("Error", f"Could not open file: {e}")
    else:
        messagebox.showerror("Error", f"File not found at:\n{file_path}\n\nIt may have been moved or deleted.")
        
def cancel_download():
    global current_process
    if current_process:
        try:
            current_process.terminate()
            logging.info("User cancelled the download process.")
            current_process = None
            return True
        except Exception as e:
            logging.error(f"Failed to kill process: {e}")
    return False

def run_download_logic(urls, options, callbacks):
    global current_process
    
    yt_path = options['yt_path']
    target_folder = options['target_folder']
    
    success_count = 0
    total_count = len(urls)

    logging.info(f"Starting batch of {total_count} downloads. Mode: {options['mode']}")

    for index, video_url in enumerate(urls):
        callbacks['status'](f"Processing {index+1}/{total_count}...", "blue")
        callbacks['progress'](0)
        
        command = [yt_path]
        
        if options['is_playlist']:
            template = f"%(playlist_title)s/%(playlist_index)s - %(title)s.%(ext)s"
            command.extend(["-o", template, "--yes-playlist"])
        elif options['custom_tmpl']:
             tmpl = options['custom_tmpl']
             if not tmpl.endswith(".%(ext)s"): tmpl += ".%(ext)s"
             command.extend(["-o", tmpl, "--no-playlist"])
        else:
            command.extend(["-o", "%(title)s.%(ext)s", "--no-playlist"])
        
        print_template = "after_move:DATA::%(filepath)s::%(title)s::%(duration_string)s::%(filesize,filesize_approx)s"
        command.extend(["--print", print_template])

        if options['use_subs']:
            command.extend(["--write-subs", "--sub-langs", "en,.*"])

        if os.path.exists(options['ff_path']): 
            command.extend(["--ffmpeg-location", options['ff_path']])

        if options['mode'] == "video":
            if options['quality'] == "Best Possible": fmt = "bv+ba/b"
            else:
                h = {"144": "144", "240": "240", "360": "360", "720": "720", "1440": "1440", "2k": "1440", "4k": "2160"}.get(options['quality'], "720")
                fmt = f"bv*[height<={h}]+ba/b[height<={h}]"
            command.extend(["-f", fmt])
            if "WebM" in options['format']: command.extend(["-S", "vcodec:vp9", "--merge-output-format", "webm"])
            else: command.extend(["-S", "vcodec:h264", "--merge-output-format", "mp4"])
        else:
            # --- ROBUST AUDIO FORMAT LOGIC ---
            raw_fmt = options['audio_fmt'].lower()
            
            # Fuzzy match the format name
            if "opus" in raw_fmt:
                tgt = "opus"
            elif "aac" in raw_fmt:
                tgt = "aac"
            elif "m4a" in raw_fmt:
                tgt = "m4a"
            elif "vorbis" in raw_fmt:
                tgt = "vorbis"
            elif "wav" in raw_fmt:
                tgt = "wav"
            else:
                tgt = "mp3" # Default if nothing else matches
            
            # Map Quality
            if "high" in raw_fmt: q = "0"
            elif "medium" in raw_fmt: q = "5"
            else: q = "10"

            command.extend(["-x", "--audio-format", tgt, "--audio-quality", q])
            
            # Metadata args...
            if options['meta_artist'] or options['meta_album']:
                meta = ""
                if options['meta_artist']: meta += f"-metadata artist=\"{options['meta_artist']}\" "
                if options['meta_album']: meta += f"-metadata album=\"{options['meta_album']}\" "
                command.extend(["--postprocessor-args", f"ffmpeg:{meta}"])

        command.append(video_url)
        # --- DEBUGGER MODE ---  #hata belki
        if options.get('debug', False):
            cmd_str = " ".join(command)
            # 1. Log to debug.log
            logging.info(f"DEBUG COMMAND: {cmd_str}")
            # 2. Print to VS Code Terminal
            print(f"\n[DEBUG] Executing:\n{cmd_str}\n")
            # 3. Save to file for easy reading
            with open("last_command.txt", "w", encoding="utf-8") as f:
                f.write(cmd_str)
        # ---------------------

        try:
            current_process = subprocess.Popen(
                command, cwd=target_folder, 
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, 
                text=True, creationflags=CREATE_NO_WINDOW, encoding='utf-8', errors='ignore'
            )
            
            while True:
                line = current_process.stdout.readline()
                if not line:
                    if current_process.poll() is not None:
                        break
                    continue

                if "[download]" in line and "%" in line:
                    match = re.search(r"(\d+\.?\d*)%", line)
                    if match:
                        try:
                            percent = float(match.group(1))
                            callbacks['progress'](percent)
                        except: pass

                if line.startswith("DATA::"):
                    parts = line.split("::")
                    if len(parts) >= 5:
                        raw_path = parts[1].strip()
                        
                        # --- CRITICAL PATH FIX: FORCE ABSOLUTE PATH ---
                        if not os.path.isabs(raw_path):
                            full_path = os.path.join(target_folder, raw_path)
                            # This converts "video.mp4" into "C:\Users\Desktop\video.mp4"
                            full_path = os.path.abspath(full_path)
                        else:
                            full_path = raw_path
                        # ----------------------------------------------

                        entry = {
                            "path": full_path,
                            "title": parts[2].strip(),
                            "duration": parts[3].strip(),
                            "size": format_size(parts[4])
                        }
                        add_to_history(entry)
                        callbacks['refresh_history']()
                        success_count += 1
                        logging.info(f"Download Success: {video_url}")

            if current_process.returncode != 0:
                 logging.error(f"Return Code {current_process.returncode}")

        except Exception as e:
            logging.critical(f"Critical System Error: {e}")
        finally:
            current_process = None

    if success_count == total_count: callbacks['finish'](True, "All Downloads Complete!")
    elif success_count > 0: callbacks['finish'](True, f"Completed {success_count}/{total_count}")
    else: callbacks['finish'](False, "Downloads failed or cancelled.")