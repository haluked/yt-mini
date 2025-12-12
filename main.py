import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import threading
import os
import config
import logic
import ui_helpers

# --- GLOBAL STATE ---
app_config = config.load_config()
current_theme = app_config.get("theme", "dark")
batch_urls = []

# --- ACTIONS ---
def toggle_theme():
    global current_theme
    current_theme = "dark" if current_theme == "light" else "light"
    app_config["theme"] = current_theme
    config.save_config(app_config)
    update_ui_theme()

def update_ui_theme():
    colors = ui_helpers.apply_theme(root, current_theme)
    btn_theme.configure(text="Switch to Light Mode" if current_theme == "dark" else "Switch to Dark Mode")
    btn_settings.configure(bg=colors["btn_bg"], fg=colors["btn_fg"])
    refresh_history_ui()

def browse_folder():
    f = filedialog.askdirectory(initialdir=entry_path.get())
    if f:
        entry_path.delete(0, tk.END)
        entry_path.insert(0, f)

def paste_clipboard():
    try:
        data = root.clipboard_get()
        entry_url.delete(0, tk.END)
        entry_url.insert(0, data)
    except: pass

def toggle_advanced():
    if var_advanced.get(): frame_advanced_options.pack(fill="x", padx=10, pady=2)
    else: frame_advanced_options.pack_forget()

def load_batch_file():
    fp = filedialog.askopenfilename(filetypes=[("Text Files", "*.txt")])
    if fp:
        with open(fp, "r") as f:
            lines = f.readlines()
            batch_urls.extend(lines)
            lbl_batch_status.config(text=f"Loaded {len(lines)} URLs")

# --- DOWNLOAD LOGIC ---
def start_download_thread():
    raw_url = entry_url.get().strip()
    main_url = ""
    if raw_url:
        if not (raw_url.startswith("http://") or raw_url.startswith("https://")):
            main_url = "https://" + raw_url
        else: main_url = raw_url

    all_urls = [main_url] if main_url else []
    for u in batch_urls: 
        if u.strip(): all_urls.append(u.strip())
    
    if not all_urls:
        lbl_status.config(text="Error: No URL provided.", fg="red")
        return

    yt_path = app_config["ytdlp_path"]
    if not os.path.exists(yt_path):
        lbl_status.config(text="Error: yt-dlp path invalid (Check Settings)", fg="red")
        return

    options = {
        'yt_path': yt_path,
        'ff_path': app_config["ffmpeg_path"],
        'target_folder': entry_path.get().strip(),
        'mode': var_mode.get(),
        'is_playlist': var_playlist.get(),
        'custom_tmpl': entry_template.get().strip() if var_advanced.get() else "",
        'use_subs': var_subs.get(),
        'format': combo_vid_format.get(),
        'quality': combo_quality.get(),
        'audio_fmt': combo_audio.get(),
        'meta_artist': entry_artist.get().strip() if var_metadata.get() else "",
        'meta_album': entry_album.get().strip() if var_metadata.get() else ""
    }

    btn_download.config(text="CANCEL", command=cancel_process, bg="#ffdddd", fg="red")
    progress_bar['value'] = 0
    
    callbacks = {
        'status': lambda msg, col: root.after(0, lambda: lbl_status.config(text=msg, fg=col if current_theme=="light" else "#4da6ff")),
        'progress': lambda val: root.after(0, lambda: progress_bar.configure(value=val)), 
        'refresh_history': lambda: root.after(0, refresh_history_ui),
        'finish': lambda success, msg: root.after(0, lambda: finish_ui_reset(success, msg))
    }

    threading.Thread(target=logic.run_download_logic, args=(all_urls, options, callbacks), daemon=True).start()

def cancel_process():
    if logic.cancel_download():
        lbl_status.config(text="Cancelling...", fg="orange")

def finish_ui_reset(success, msg):
    lbl_status.config(text=msg, fg="green" if success else "red")
    colors = config.THEMES[current_theme]
    btn_download.config(text="EXECUTE DOWNLOAD", command=start_download_thread, bg=colors["btn_bg"], fg=colors["btn_fg"])
    batch_urls.clear()
    lbl_batch_status.config(text="")
    progress_bar['value'] = 0
    refresh_history_ui()

# --- HISTORY ---
def refresh_history_ui():
    for w in scrollable_frame.winfo_children(): w.destroy()
    history = config.load_history()
    colors = config.THEMES[current_theme]

    if not history:
        tk.Label(scrollable_frame, text="No recent downloads.", font=("Arial", 10, "italic"), bg=colors["bg"], fg=colors["fg"]).pack(pady=20)
        return

    for item in history:
        ui_helpers.create_history_card(
            scrollable_frame, 
            item, 
            colors, 
            open_file_cmd=logic.open_file_safe,
            open_folder_cmd=logic.open_folder_safe,
            delete_cmd=delete_ui_action
        )

def delete_ui_action(entry):
    if messagebox.askyesno("Delete", f"Delete {os.path.basename(entry['path'])}?"):
        if os.path.exists(entry['path']):
            try: os.remove(entry['path'])
            except: pass
        hist = config.load_history()
        hist = [x for x in hist if x['path'] != entry['path']]
        config.save_history_list(hist)
        refresh_history_ui()

def delete_all_action():
    if not config.load_history(): return # Nothing to delete
    
    if messagebox.askyesno("Clear History", "Clear the entire history list?\n\n(Downloaded files will NOT be deleted from your disk)"):
        config.save_history_list([])
        refresh_history_ui()

# --- SETTINGS LOGIC ---
def do_autodetect():
    yt = logic.auto_detect_ytdlp()
    if yt: 
        entry_ytdlp.delete(0, tk.END); entry_ytdlp.insert(0, yt)
    
    ff_list = logic.get_all_ffmpeg_paths()
    if ff_list:
        combo_ffmpeg['values'] = ff_list
        best_choice = ff_list[0]
        for p in ff_list:
            if "solidworks" in p.lower(): continue
            if "gyan" in p.lower() or "shared" in p.lower() or "winget" in p.lower():
                best_choice = p; break
        combo_ffmpeg.set(best_choice)
        messagebox.showinfo("Success", f"Found {len(ff_list)} paths.\nSelected: {os.path.basename(os.path.dirname(best_choice))}")
    else: messagebox.showinfo("Result", "yt-dlp found, but FFmpeg missing.")

def do_save_settings():
    app_config["ytdlp_path"] = entry_ytdlp.get().strip()
    app_config["ffmpeg_path"] = combo_ffmpeg.get().strip()
    config.save_config(app_config)
    show_main()

def show_settings():
    frame_main.pack_forget(); frame_settings.pack(fill="both", expand=True)
def show_main():
    frame_settings.pack_forget(); frame_main.pack(fill="both", expand=True)
    app_config.update(config.load_config())
def do_reset():
    if messagebox.askyesno("Reset", "Delete all settings?"):
        config.factory_reset(); root.destroy(); os._exit(0)

# ================= LAYOUT =================
root = tk.Tk()
root.title(f"yt-mini v{config.VERSION}")
root.geometry("600x680")
root.minsize(600, 500)

frame_main = tk.Frame(root)
frame_main.pack(fill="both", expand=True)

# Header
frame_top = tk.Frame(frame_main); frame_top.pack(side="top", fill="x")
header = tk.Frame(frame_top); header.pack(fill="x", padx=10, pady=5)
btn_settings = tk.Button(header, text="⚙️ Setup", command=show_settings); btn_settings.pack(side="left")
btn_theme = tk.Button(header, text="Switch Theme", command=toggle_theme); btn_theme.pack(side="right")

# URL
url_cont = tk.Frame(frame_top); url_cont.pack(pady=2)
tk.Label(url_cont, text="Paste Link:").pack(anchor="w")
frame_u = tk.Frame(url_cont); frame_u.pack()
entry_url = tk.Entry(frame_u, width=65); entry_url.pack(side="left")
btn_p = tk.Button(frame_u, text="📋", command=paste_clipboard, width=3); btn_p.pack(side="left", padx=5)
ui_helpers.create_tooltip(btn_p, config.TOOLTIPS["paste"])

# Path
path_cont = tk.Frame(frame_top); path_cont.pack(pady=2)
tk.Label(path_cont, text="Save to:").pack(side="left")
entry_path = tk.Entry(path_cont, width=45); entry_path.insert(0, app_config["download_path"]); entry_path.pack(side="left", padx=5)
btn_b = tk.Button(path_cont, text="Browse", command=browse_folder); btn_b.pack(side="left")
ui_helpers.create_tooltip(btn_b, config.TOOLTIPS["browse"])

# Options
opt_cont = tk.Frame(frame_top); opt_cont.pack(pady=2)
var_playlist = tk.BooleanVar()
chk_pl = tk.Checkbutton(opt_cont, text="Playlist (Auto-Create Folder)", variable=var_playlist); chk_pl.pack(anchor="w")
ui_helpers.create_tooltip(chk_pl, config.TOOLTIPS["playlist"])
var_subs = tk.BooleanVar()
chk_s = tk.Checkbutton(opt_cont, text="Download Subtitles", variable=var_subs); chk_s.pack(anchor="w")
ui_helpers.create_tooltip(chk_s, config.TOOLTIPS["subs"])

# Advanced
var_advanced = tk.BooleanVar()
chk_adv = tk.Checkbutton(frame_top, text="Show Advanced Options", variable=var_advanced, command=toggle_advanced, fg="blue")
chk_adv.pack(pady=2)
ui_helpers.create_tooltip(chk_adv, config.TOOLTIPS["advanced"])
frame_advanced_options = tk.Frame(frame_top, highlightbackground="gray", highlightthickness=1)
tk.Label(frame_advanced_options, text="Filename Template:", font=("Arial", 8)).pack(anchor="w", padx=5)
entry_template = tk.Entry(frame_advanced_options, width=50); entry_template.pack(pady=2)
tk.Button(frame_advanced_options, text="Import .txt (Batch)", command=load_batch_file).pack(pady=2)
lbl_batch_status = tk.Label(frame_advanced_options, text="", fg="green"); lbl_batch_status.pack()

# Mode Selection
var_mode = tk.StringVar(value="video")
mode_f = tk.Frame(frame_top); mode_f.pack(pady=5)
def update_vis():
    if var_mode.get() == "video": audio_c.pack_forget(); video_c.pack(pady=2)
    else: video_c.pack_forget(); audio_c.pack(pady=2)
tk.Radiobutton(mode_f, text="Video", variable=var_mode, value="video", command=update_vis).pack(side="left", padx=20)
tk.Radiobutton(mode_f, text="Sound", variable=var_mode, value="sound", command=update_vis).pack(side="left", padx=20)

# Video Config
video_c = tk.Frame(frame_top)
tk.Label(video_c, text="Format:").grid(row=0, column=0)
combo_vid_format = ttk.Combobox(video_c, values=["WebM (VP9)", "MP4 (H264)"], state="readonly", width=20); combo_vid_format.grid(row=0, column=1, padx=5); combo_vid_format.current(0)
ui_helpers.create_tooltip(combo_vid_format, config.TOOLTIPS["format"])
tk.Label(video_c, text="Quality:").grid(row=0, column=2)
combo_quality = ttk.Combobox(video_c, values=["144", "240", "360", "720", "1440", "2k", "4k", "Best"], state="readonly", width=10); combo_quality.grid(row=0, column=3, padx=5); combo_quality.current(3)
ui_helpers.create_tooltip(combo_quality, config.TOOLTIPS["quality"])

# Audio Config
audio_c = tk.Frame(frame_top)
combo_audio = ttk.Combobox(audio_c, values=["MP3 - High", "MP3 - Medium", "Opus - High", "AAC - High"], state="readonly", width=25); combo_audio.current(0); combo_audio.pack(pady=5)
ui_helpers.create_tooltip(combo_audio, config.TOOLTIPS["audio_fmt"])
frame_m = tk.Frame(audio_c); frame_m.pack()
var_metadata = tk.BooleanVar()
chk_m = tk.Checkbutton(frame_m, text="Tag Metadata", variable=var_metadata); chk_m.grid(row=0, column=0, columnspan=2, sticky="w")
ui_helpers.create_tooltip(chk_m, config.TOOLTIPS["meta"])
tk.Label(frame_m, text="Artist:").grid(row=1, column=0); entry_artist = tk.Entry(frame_m, width=15); entry_artist.grid(row=1, column=1)
tk.Label(frame_m, text="Album:").grid(row=2, column=0); entry_album = tk.Entry(frame_m, width=15); entry_album.grid(row=2, column=1)

# Footer
progress_bar = ttk.Progressbar(frame_main, orient="horizontal", length=500, mode="determinate"); progress_bar.pack(side="bottom", pady=(0, 5))
lbl_status = tk.Label(frame_main, text="Ready"); lbl_status.pack(side="bottom", pady=2)
btn_download = tk.Button(frame_main, text="EXECUTE DOWNLOAD", command=start_download_thread, height=2, width=20); btn_download.pack(side="bottom", pady=10)

# History List
hist_head = tk.Frame(frame_main)
hist_head.pack(side="top", fill="x", padx=20, pady=(10, 0)) # New Header Frame
tk.Label(hist_head, text="Recent Downloads", font=("Arial", 10, "bold")).pack(side="left")
tk.Button(hist_head, text="Clear History", font=("Arial", 7), command=delete_all_action, bg="#ffdddd", fg="red").pack(side="right")

cont = tk.Frame(frame_main); cont.pack(side="top", fill="both", expand=True, padx=10, pady=(0, 10))
canv = tk.Canvas(cont, highlightthickness=0); scr = tk.Scrollbar(cont, command=canv.yview)
scrollable_frame = tk.Frame(canv)
scrollable_frame.bind("<Configure>", lambda e: canv.configure(scrollregion=canv.bbox("all")))
canv.create_window((0, 0), window=scrollable_frame, anchor="nw")
canv.configure(yscrollcommand=scr.set)
canv.pack(side="left", fill="both", expand=True); scr.pack(side="right", fill="y")
def _scroll(e): canv.yview_scroll(int(-1*(e.delta/120)), "units")
canv.bind_all("<MouseWheel>", _scroll)

# Settings Frame
frame_settings = tk.Frame(root)
tk.Label(frame_settings, text="Settings & Setup", font=("Arial", 12, "bold")).pack(pady=20)
tk.Label(frame_settings, text="Use 'Auto-Detect' to find installed tools.", fg="gray").pack()
fs_yt = tk.Frame(frame_settings); fs_yt.pack(fill="x", padx=20, pady=5)
tk.Label(fs_yt, text="yt-dlp:", width=10, anchor="w").pack(side="left")
entry_ytdlp = tk.Entry(fs_yt); entry_ytdlp.insert(0, app_config["ytdlp_path"]); entry_ytdlp.pack(side="left", fill="x", expand=True)
fs_ff = tk.Frame(frame_settings); fs_ff.pack(fill="x", padx=20, pady=5)
tk.Label(fs_ff, text="FFmpeg:", width=10, anchor="w").pack(side="left")
combo_ffmpeg = ttk.Combobox(fs_ff); combo_ffmpeg.set(app_config["ffmpeg_path"]); combo_ffmpeg.pack(side="left", fill="x", expand=True)
fs_inst = tk.Frame(frame_settings); fs_inst.pack(pady=10)
tk.Button(fs_inst, text="Install yt-dlp", command=lambda: logic.install_via_winget("yt-dlp")).pack(side="left", padx=5)
tk.Button(fs_inst, text="Install FFmpeg", command=lambda: logic.install_via_winget("Gyan.FFmpeg")).pack(side="left", padx=5)
tk.Button(frame_settings, text="Auto-Detect Paths", command=do_autodetect).pack(pady=5)
tk.Button(frame_settings, text="Check Updates", command=lambda: logic.update_tools(entry_ytdlp.get())).pack(pady=5)
tk.Button(frame_settings, text="Save & Return", command=do_save_settings, bg="#4CAF50", fg="white").pack(pady=20)
tk.Button(frame_settings, text="Cancel", command=show_main).pack()
tk.Button(frame_settings, text="⚠️ Factory Reset", command=do_reset, bg="#ffdddd", fg="red").pack(pady=20, side="bottom")

update_ui_theme()
update_vis()
refresh_history_ui()
root.mainloop()