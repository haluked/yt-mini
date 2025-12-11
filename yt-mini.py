import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import subprocess
import os
import shutil
import json
import threading

# --- CONSTANTS & DEFAULTS ---
CONFIG_FILE = "config.txt"
HISTORY_FILE = "history.json"
DEFAULT_CONFIG = {
    "ytdlp_path": "",
    "ffmpeg_path": "",
    "download_path": os.path.join(os.path.expanduser("~"), "Desktop"),
    "theme": "dark"
}

# --- THEME SETTINGS ---
THEMES = {
    "light": {
        "bg": "#f0f0f0", "fg": "#000000",
        "entry_bg": "#ffffff", "entry_fg": "#000000",
        "btn_bg": "#e0e0e0", "btn_fg": "#000000",
        "status_fg": "#333333",
        "card_bg": "#ffffff", "card_border": "#cccccc"
    },
    "dark": {
        "bg": "#2d2d2d", "fg": "#ffffff",
        "entry_bg": "#404040", "entry_fg": "#ffffff",
        "btn_bg": "#505050", "btn_fg": "#ffffff",
        "status_fg": "#cccccc",
        "card_bg": "#383838", "card_border": "#555555"
    }
}
current_theme = "dark"

# --- DATA MANAGERS ---
def load_config():
    config = DEFAULT_CONFIG.copy()
    if os.path.exists(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, "r") as f:
                for line in f:
                    if "=" in line:
                        key, value = line.strip().split("=", 1)
                        if key in config:
                            config[key] = value
        except: pass
    return config

def save_config(config_data):
    with open(CONFIG_FILE, "w") as f:
        for key, value in config_data.items():
            f.write(f"{key}={value}\n")

def load_history():
    if os.path.exists(HISTORY_FILE):
        try:
            with open(HISTORY_FILE, "r") as f:
                return json.load(f)
        except: return []
    return []

def save_history_list(history_list):
    with open(HISTORY_FILE, "w") as f:
        json.dump(history_list, f, indent=4)

def add_to_history(entry):
    history = load_history()
    history = [x for x in history if x['path'] != entry['path']]
    history.insert(0, entry)
    history = history[:50]
    save_history_list(history)

# --- UTILS ---
def install_via_winget(package_id):
    try:
        subprocess.run(["start", "cmd", "/k", f"winget install {package_id} && exit"], shell=True)
    except Exception as e:
        messagebox.showerror("Error", f"Could not launch installer: {e}")

def update_tools(yt_path):
    try:
        msg = "Update process started...\n\n"
        if os.path.exists(yt_path):
            subprocess.run([yt_path, "-U"], shell=False)
            msg += "✅ yt-dlp update command sent.\n"
        else:
            msg += "❌ yt-dlp not found.\n"
        subprocess.run(["start", "cmd", "/k", "winget upgrade Gyan.FFmpeg && exit"], shell=True)
        msg += "✅ FFmpeg update launched in new window.\n"
        messagebox.showinfo("Updater", msg)
    except Exception as e:
        messagebox.showerror("Error", str(e))

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

def auto_detect_ytdlp():
    path = shutil.which("yt-dlp")
    if not path:
        winget_path = os.path.expanduser(r"~\AppData\Local\Microsoft\WinGet\Packages")
        for root, dirs, files in os.walk(winget_path):
            if "yt-dlp.exe" in files: return os.path.join(root, "yt-dlp.exe")
    return path or ""

def format_size(size_bytes):
    try:
        s = float(size_bytes)
        for unit in ['B', 'KB', 'MB', 'GB']:
            if s < 1024.0: return f"{s:.1f} {unit}"
            s /= 1024.0
        return f"{s:.1f} TB"
    except: return "N/A"

# --- CORE LOGIC ---
def run_download():
    btn_download.config(state="disabled")
    threading.Thread(target=download_process, daemon=True).start()

def download_process():
    all_urls = []
    main_url = entry_url.get().strip()
    if main_url: all_urls.append(main_url)
    
    for u in batch_urls:
        if u.strip(): all_urls.append(u.strip())

    if not all_urls:
        return finish_download(False, "Error: No URLs provided")

    target_folder = entry_path.get().strip()
    mode = var_mode.get()
    is_playlist = var_playlist.get()
    is_advanced = var_advanced.get()
    use_subs = var_subs.get()
    
    current_conf = load_config()
    yt_path = current_conf["ytdlp_path"]
    ff_path = current_conf["ffmpeg_path"]

    if not os.path.exists(yt_path): return finish_download(False, "Error: yt-dlp path invalid")
    
    app_config["download_path"] = target_folder
    save_config(app_config)
    
    success_count = 0
    total_count = len(all_urls)

    for index, video_url in enumerate(all_urls):
        update_status(f"Processing {index+1}/{total_count}...", "blue")
        command = [yt_path]
        
        if is_playlist:
            template = f"%(playlist_title)s/%(playlist_index)s - %(title)s.%(ext)s"
            command.extend(["-o", template, "--yes-playlist"])
        elif is_advanced and entry_template.get().strip():
             custom_tmpl = entry_template.get().strip()
             if not custom_tmpl.endswith(".%(ext)s"): custom_tmpl += ".%(ext)s"
             command.extend(["-o", custom_tmpl, "--no-playlist"])
        else:
            command.extend(["-o", "%(title)s.%(ext)s", "--no-playlist"])
        
        print_template = "after_move:DATA::%(filepath)s::%(title)s::%(duration_string)s::%(filesize,filesize_approx)s"
        command.extend(["--print", print_template])

        if use_subs:
            command.extend(["--write-subs", "--sub-langs", "en,.*"])

        if os.path.exists(ff_path): command.extend(["--ffmpeg-location", ff_path])
        elif mode == "sound": return finish_download(False, "Error: FFmpeg needed for audio")

        if mode == "video":
            quality_sel = combo_quality.get()
            format_sel = combo_vid_format.get()
            height_map = {"144": "144", "240": "240", "360": "360", "720": "720", "1440": "1440", "2k": "1440", "4k": "2160"}
            
            if quality_sel == "Best Possible": fmt_str = "bv+ba/b"
            else:
                h = height_map.get(quality_sel, "720")
                fmt_str = f"bv*[height<={h}]+ba/b[height<={h}]"
            command.extend(["-f", fmt_str])
            if "WebM" in format_sel: command.extend(["-S", "vcodec:vp9", "--merge-output-format", "webm"])
            else: command.extend(["-S", "vcodec:h264", "--merge-output-format", "mp4"])
        else:
            audio_sel = combo_audio.get()
            target_fmt = audio_sel.split(" - ")[0].lower()
            if "High" in audio_sel: q_val = "0"
            elif "Medium" in audio_sel: q_val = "5"
            else: q_val = "10"
            command.extend(["-x", "--audio-format", target_fmt, "--audio-quality", q_val])
            
            if var_metadata.get():
                artist = entry_artist.get().strip()
                album = entry_album.get().strip()
                if artist or album:
                    meta_args = ""
                    if artist: meta_args += f"-metadata artist=\"{artist}\" "
                    if album: meta_args += f"-metadata album=\"{album}\" "
                    command.extend(["--postprocessor-args", f"ffmpeg:{meta_args}"])

        command.append(video_url)

        try:
            process = subprocess.Popen(
                command, cwd=target_folder, stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
                text=True, creationflags=0x08000000, encoding='utf-8', errors='ignore'
            )
            stdout, stderr = process.communicate()
            
            if process.returncode == 0:
                success_count += 1
                for line in stdout.splitlines():
                    if line.startswith("DATA::"):
                        parts = line.split("::")
                        if len(parts) >= 5:
                            entry = {
                                "path": parts[1].strip(),
                                "title": parts[2].strip(),
                                "duration": parts[3].strip(),
                                "size": format_size(parts[4])
                            }
                            add_to_history(entry)
            else:
                print(f"Error on {video_url}: {stderr}")
        except Exception as e:
            print(f"Sys Error: {e}")

    reset_advanced_ui_threadsafe()
    
    if success_count == total_count:
        finish_download(True, "All Downloads Complete!")
    elif success_count > 0:
        finish_download(True, f"Completed {success_count}/{total_count}")
    else:
        finish_download(False, "All downloads failed.")

def update_status(msg, color_name):
    def _update():
        color = color_name
        if current_theme == "dark":
            if color == "blue": color = "#4da6ff"
            if color == "green": color = "#00ff00"
        lbl_status.config(text=msg, fg=color)
    root.after(0, _update)

def finish_download(success, msg):
    color = "green" if success else "red"
    update_status(msg, color)
    root.after(0, lambda: btn_download.config(state="normal"))
    if success: root.after(0, refresh_history_ui)

def reset_advanced_ui_threadsafe():
    def _reset():
        batch_urls.clear()
        lbl_batch_status.config(text="")
    root.after(0, _reset)

# --- ACTION FUNCTIONS ---
def delete_file(entry):
    path = entry['path']
    if messagebox.askyesno("Delete File", f"Permanently delete this file?\n\n{os.path.basename(path)}"):
        if os.path.exists(path):
            try: os.remove(path)
            except Exception as e: messagebox.showerror("Error", f"Could not delete file: {e}")
        hide_entry(entry)

def hide_entry(entry):
    history = load_history()
    new_history = [x for x in history if x['path'] != entry['path']]
    save_history_list(new_history)
    refresh_history_ui()

def open_file_path(path):
    path = path.strip()
    if os.path.exists(path):
        try: os.startfile(path)
        except Exception as e: messagebox.showerror("Error", str(e))
    else: messagebox.showerror("Error", f"File not found:\n{path}")

def open_folder_path(file_path):
    file_path = file_path.strip()
    if os.path.exists(file_path): subprocess.run(f'explorer /select,"{file_path}"')
    else:
        folder = os.path.dirname(file_path)
        if os.path.exists(folder): os.startfile(folder)
        else: messagebox.showerror("Error", "Folder not found.")

def refresh_history_ui():
    for widget in scrollable_frame.winfo_children(): widget.destroy()
    history = load_history()
    colors = THEMES[current_theme]

    if not history:
        tk.Label(scrollable_frame, text="No recent downloads.", font=("Arial", 10, "italic"), bg=colors["bg"], fg=colors["fg"]).pack(pady=20)
        return

    for item in history:
        card = tk.Frame(scrollable_frame, bg=colors["card_bg"], highlightbackground=colors["card_border"], highlightthickness=1)
        card.pack(fill="x", padx=10, pady=5, ipady=5)

        info_frame = tk.Frame(card, bg=colors["card_bg"])
        info_frame.pack(side="left", fill="both", expand=True, padx=10)

        title_btn = tk.Button(info_frame, text=item["title"], anchor="w", font=("Arial", 9, "bold"),
                              bg=colors["card_bg"], fg=colors["fg"], borderwidth=0, activebackground=colors["card_bg"],
                              command=lambda p=item["path"]: open_file_path(p), cursor="hand2")
        title_btn.pack(fill="x")
        
        details_txt = f"{item['duration']}  |  {item['size']}"
        tk.Label(info_frame, text=details_txt, anchor="w", font=("Arial", 8), bg=colors["card_bg"], fg=colors["status_fg"]).pack(fill="x")

        btn_frame = tk.Frame(card, bg=colors["card_bg"])
        btn_frame.pack(side="right", padx=5)

        tk.Button(btn_frame, text="📂", font=("Segoe UI Emoji", 9), width=3, bg=colors["btn_bg"], fg=colors["btn_fg"],
                  command=lambda p=item["path"]: open_folder_path(p)).pack(side="left", padx=2)
        tk.Button(btn_frame, text="👁️‍🗨️", font=("Segoe UI Emoji", 9), width=3, bg=colors["btn_bg"], fg=colors["btn_fg"],
                  command=lambda i=item: hide_entry(i)).pack(side="left", padx=2)
        tk.Button(btn_frame, text="🗑️", font=("Segoe UI Emoji", 9), width=3, bg="#ffdddd", fg="red",
                  command=lambda i=item: delete_file(i)).pack(side="left", padx=2)

# --- GUI FUNCTIONS ---
def toggle_theme():
    global current_theme
    current_theme = "dark" if current_theme == "light" else "light"
    app_config["theme"] = current_theme
    save_config(app_config)
    apply_theme()

def apply_theme():
    colors = THEMES[current_theme]
    root.configure(bg=colors["bg"])
    
    def update_widget(widget):
        try:
            w_type = widget.winfo_class()
            if w_type == 'Label': widget.configure(bg=colors["bg"], fg=colors["fg"])
            elif w_type == 'Button': 
                if widget.cget('text') not in ["🗑️", "⚠️ Factory Reset App", "Save & Return"]:
                    widget.configure(bg=colors["btn_bg"], fg=colors["btn_fg"], activebackground=colors["btn_bg"])
            elif w_type == 'Entry': widget.configure(bg=colors["entry_bg"], fg=colors["entry_fg"], insertbackground=colors["fg"])
            elif w_type == 'Checkbutton' or w_type == 'Radiobutton': widget.configure(bg=colors["bg"], fg=colors["fg"], selectcolor=colors["bg"], activebackground=colors["bg"])
            elif w_type == 'Frame' or w_type == 'Labelframe' or w_type == 'Canvas': widget.configure(bg=colors["bg"])
            for child in widget.winfo_children(): update_widget(child)
        except: pass

    update_widget(root)
    btn_theme.configure(text="Switch to Light Mode" if current_theme == "dark" else "Switch to Dark Mode")
    btn_settings.configure(bg=colors["btn_bg"], fg=colors["btn_fg"])
    refresh_history_ui()

def browse_folder():
    folder = filedialog.askdirectory(initialdir=entry_path.get())
    if folder:
        entry_path.delete(0, tk.END)
        entry_path.insert(0, folder)

def paste_clipboard():
    try:
        data = root.clipboard_get()
        entry_url.delete(0, tk.END)
        entry_url.insert(0, data)
    except: pass

def toggle_advanced():
    if var_advanced.get():
        frame_advanced_options.pack(fill="x", padx=10, pady=2)
    else:
        frame_advanced_options.pack_forget()

def load_batch_file():
    filepath = filedialog.askopenfilename(filetypes=[("Text Files", "*.txt")])
    if filepath:
        with open(filepath, "r") as f:
            lines = f.readlines()
            batch_urls.extend(lines)
            lbl_batch_status.config(text=f"Loaded {len(lines)} URLs")

def factory_reset():
    if messagebox.askyesno("Factory Reset", "Are you sure?\n\nThis will delete your history and configuration settings.\nThe app will close immediately."):
        try:
            if os.path.exists(CONFIG_FILE): os.remove(CONFIG_FILE)
            if os.path.exists(HISTORY_FILE): os.remove(HISTORY_FILE)
            root.destroy()
            os._exit(0)
        except Exception as e: messagebox.showerror("Error", str(e))

# --- PAGE NAVIGATION ---
def show_settings():
    frame_main.pack_forget()
    frame_settings.pack(fill="both", expand=True)

def show_main():
    frame_settings.pack_forget()
    frame_main.pack(fill="both", expand=True)
    app_config.update(load_config()) 

# --- GUI SETUP ---
root = tk.Tk()
root.title("yt-mini")
root.geometry("600x650") 
root.minsize(600, 500)

app_config = load_config()

# ================= MAIN FRAME =================
frame_main = tk.Frame(root)
frame_main.pack(fill="both", expand=True)

# 1. TOP SECTION (Fixed)
frame_top = tk.Frame(frame_main)
frame_top.pack(side="top", fill="x")

# Header
header_main = tk.Frame(frame_top)
header_main.pack(fill="x", padx=10, pady=5)
btn_settings = tk.Button(header_main, text="⚙️ Setup", command=show_settings)
btn_settings.pack(side="left")
btn_theme = tk.Button(header_main, text="Switch Theme", command=toggle_theme)
btn_theme.pack(side="right")

# URL
url_frame_container = tk.Frame(frame_top)
url_frame_container.pack(pady=2)
lbl_url = tk.Label(url_frame_container, text="Paste YouTube Link:")
lbl_url.pack(anchor="w")
frame_url_input = tk.Frame(url_frame_container)
frame_url_input.pack()
entry_url = tk.Entry(frame_url_input, width=65)
entry_url.pack(side="left")
btn_paste = tk.Button(frame_url_input, text="📋", command=paste_clipboard, width=3)
btn_paste.pack(side="left", padx=5)

# Path
path_frame = tk.Frame(frame_top)
path_frame.pack(pady=2)
tk.Label(path_frame, text="Save to:").pack(side="left")
entry_path = tk.Entry(path_frame, width=45)
entry_path.insert(0, app_config["download_path"])
entry_path.pack(side="left", padx=5)
btn_browse = tk.Button(path_frame, text="Browse", command=browse_folder)
btn_browse.pack(side="left")

# Options
options_frame = tk.Frame(frame_top)
options_frame.pack(pady=2)
var_playlist = tk.BooleanVar()
chk_playlist = tk.Checkbutton(options_frame, text="Playlist (Auto-Create Folder)", variable=var_playlist)
chk_playlist.pack(anchor="w")
var_subs = tk.BooleanVar()
chk_subs = tk.Checkbutton(options_frame, text="Download Subtitles (En+Orig)", variable=var_subs)
chk_subs.pack(anchor="w")

# Advanced
var_advanced = tk.BooleanVar()
chk_advanced = tk.Checkbutton(frame_top, text="Show Advanced Options", 
                              variable=var_advanced, command=toggle_advanced, fg="blue")
chk_advanced.pack(pady=2)

frame_advanced_options = tk.Frame(frame_top, highlightbackground="gray", highlightthickness=1)
batch_urls = []
tk.Label(frame_advanced_options, text="Filename Template (Optional):", font=("Arial", 8)).pack(anchor="w", padx=5)
entry_template = tk.Entry(frame_advanced_options, width=50)
entry_template.pack(pady=2)
tk.Label(frame_advanced_options, text="e.g. %(uploader)s - %(title)s", font=("Arial", 7, "italic"), fg="gray").pack()
btn_batch = tk.Button(frame_advanced_options, text="Import .txt File (Batch)", command=load_batch_file)
btn_batch.pack(pady=5)
lbl_batch_status = tk.Label(frame_advanced_options, text="", fg="green")
lbl_batch_status.pack()

# Mode
var_mode = tk.StringVar(value="video")
mode_frame = tk.Frame(frame_top)
mode_frame.pack(pady=5)
def update_ui_visibility():
    if var_mode.get() == "video":
        audio_container.pack_forget()
        video_container.pack(pady=2)
    else:
        video_container.pack_forget()
        audio_container.pack(pady=2)
tk.Radiobutton(mode_frame, text="Video", variable=var_mode, value="video", command=update_ui_visibility).pack(side="left", padx=20)
tk.Radiobutton(mode_frame, text="Sound", variable=var_mode, value="sound", command=update_ui_visibility).pack(side="left", padx=20)

# Video Config
video_container = tk.Frame(frame_top)
tk.Label(video_container, text="Format:").grid(row=0, column=0)
combo_vid_format = ttk.Combobox(video_container, values=["WebM (VP9 - Efficient)", "MP4 (H264 - Compatible)"], state="readonly", width=25)
combo_vid_format.grid(row=0, column=1, padx=5)
combo_vid_format.current(0)
tk.Label(video_container, text="Quality:").grid(row=0, column=2)
combo_quality = ttk.Combobox(video_container, values=["144", "240", "360", "720", "1440", "2k", "4k", "Best Possible"], state="readonly", width=15)
combo_quality.grid(row=0, column=3, padx=5)
combo_quality.current(3)

# Audio Config
audio_container = tk.Frame(frame_top)
audio_options = ["MP3 - High (~320kbps)", "MP3 - Medium (~128kbps)", "MP3 - Low (~64kbps)", "Opus - High (~160kbps)", "Opus - Medium (~96kbps)", "Opus - Low (~48kbps)", "AAC - High (~256kbps)", "AAC - Medium (~128kbps)", "AAC - Low (~64kbps)"]
combo_audio = ttk.Combobox(audio_container, values=audio_options, state="readonly", width=30); combo_audio.current(0); combo_audio.pack(pady=5)

frame_meta = tk.Frame(audio_container)
frame_meta.pack(pady=5)
var_metadata = tk.BooleanVar()
chk_meta = tk.Checkbutton(frame_meta, text="Tag Metadata", variable=var_metadata)
chk_meta.grid(row=0, column=0, columnspan=2, sticky="w")
tk.Label(frame_meta, text="Artist:").grid(row=1, column=0, sticky="e")
entry_artist = tk.Entry(frame_meta, width=20)
entry_artist.grid(row=1, column=1, padx=5)
tk.Label(frame_meta, text="Album:").grid(row=2, column=0, sticky="e")
entry_album = tk.Entry(frame_meta, width=20)
entry_album.grid(row=2, column=1, padx=5)

# 2. BOTTOM SECTION (Fixed at bottom)
lbl_status = tk.Label(frame_main, text="Ready")
lbl_status.pack(side="bottom", pady=2)

btn_download = tk.Button(frame_main, text="EXECUTE DOWNLOAD", command=run_download, height=2, width=20)
btn_download.pack(side="bottom", pady=10)

# 3. MIDDLE SECTION (History - Fills remaining space)
tk.Label(frame_main, text="Recent Downloads", font=("Arial", 10, "bold")).pack(side="top", pady=(5, 5), anchor="w", padx=20)

container = tk.Frame(frame_main)
container.pack(side="top", fill="both", expand=True, padx=10, pady=(0, 10))

canvas_history = tk.Canvas(container, highlightthickness=0)
scrollbar = tk.Scrollbar(container, orient="vertical", command=canvas_history.yview)
scrollable_frame = tk.Frame(canvas_history)
scrollable_frame.bind("<Configure>", lambda e: canvas_history.configure(scrollregion=canvas_history.bbox("all")))
canvas_history.create_window((0, 0), window=scrollable_frame, anchor="nw")
canvas_history.configure(yscrollcommand=scrollbar.set)
canvas_history.pack(side="left", fill="both", expand=True)
scrollbar.pack(side="right", fill="y")
def _on_mousewheel(event): canvas_history.yview_scroll(int(-1*(event.delta/120)), "units")
canvas_history.bind_all("<MouseWheel>", _on_mousewheel)

# ================= SETTINGS FRAME =================
frame_settings = tk.Frame(root)

tk.Label(frame_settings, text="Settings & Setup", font=("Arial", 12, "bold")).pack(pady=(20, 10))
explanation_text = ("• yt-dlp downloads the streams.\n• FFmpeg merges them.\n\nINSTRUCTIONS:\n1. Click Install buttons below.\n2. Click 'Auto-Detect Paths'.\n3. Click 'Update Tools' periodically.")
lbl_explain = tk.Label(frame_settings, text=explanation_text, justify="left", font=("Arial", 9), fg="#888888")
lbl_explain.pack(pady=(0, 20), padx=20, anchor="w")

frame_yt = tk.Frame(frame_settings)
frame_yt.pack(fill="x", padx=20, pady=5)
tk.Label(frame_yt, text="yt-dlp Path:", width=15, anchor="w").pack(side="left")
entry_ytdlp = tk.Entry(frame_yt)
entry_ytdlp.insert(0, app_config["ytdlp_path"])
entry_ytdlp.pack(side="left", fill="x", expand=True, padx=5)

frame_ff = tk.Frame(frame_settings)
frame_ff.pack(fill="x", padx=20, pady=5)
tk.Label(frame_ff, text="FFmpeg Path:", width=15, anchor="w").pack(side="left")
combo_ffmpeg = ttk.Combobox(frame_ff)
combo_ffmpeg.set(app_config["ffmpeg_path"])
combo_ffmpeg.pack(side="left", fill="x", expand=True, padx=5)

frame_install = tk.Frame(frame_settings)
frame_install.pack(pady=20)
def do_install_ytdlp(): install_via_winget("yt-dlp")
def do_install_ffmpeg(): install_via_winget("Gyan.FFmpeg")
tk.Button(frame_install, text="Install yt-dlp", command=do_install_ytdlp).pack(side="left", padx=5)
tk.Button(frame_install, text="Install FFmpeg", command=do_install_ffmpeg).pack(side="left", padx=5)

def do_autodetect():
    yt_found = auto_detect_ytdlp()
    if yt_found:
        entry_ytdlp.delete(0, tk.END)
        entry_ytdlp.insert(0, yt_found)
    ff_list = get_all_ffmpeg_paths()
    if ff_list:
        combo_ffmpeg['values'] = ff_list 
        best_choice = ff_list[0]
        for p in ff_list:
            if "gyan" in p.lower() or "shared" in p.lower(): best_choice = p; break
        combo_ffmpeg.set(best_choice)
        messagebox.showinfo("Auto-Detect", f"Found {len(ff_list)} FFmpeg paths.\nSelected: {best_choice}")
    else: messagebox.showinfo("Auto-Detect", "yt-dlp found.\nNo FFmpeg found.")

tk.Button(frame_settings, text="Auto-Detect Paths", command=do_autodetect, width=30).pack(pady=5)

def do_update():
    update_tools(entry_ytdlp.get().strip())
tk.Button(frame_settings, text="🔄 Check for Updates (yt-dlp & FFmpeg)", command=do_update, width=35, bg="#e0e0e0").pack(pady=15)

def do_save():
    app_config["ytdlp_path"] = entry_ytdlp.get().strip()
    app_config["ffmpeg_path"] = combo_ffmpeg.get().strip()
    save_config(app_config)
    lbl_status.config(text="Settings Saved.", fg="green")
    show_main()

tk.Button(frame_settings, text="Save & Return", command=do_save, height=2, width=20, bg="#4CAF50", fg="white").pack(pady=20)
tk.Button(frame_settings, text="Cancel / Return Back", command=show_main).pack(pady=5)
tk.Button(frame_settings, text="⚠️ Factory Reset App", command=factory_reset, bg="#ffdddd", fg="red").pack(pady=20, side="bottom")

# ================= INIT =================
current_theme = app_config.get("theme", "light")
apply_theme()
update_ui_visibility()
refresh_history_ui()

root.mainloop()