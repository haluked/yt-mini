import customtkinter as ctk
from tkinter import filedialog, messagebox
import tkinter as tk
import threading
import os
import config
import logic
import ui_helpers

# --- SETUP CUSTOMTKINTER ---
ctk.set_appearance_mode("Dark")  # Default mode
ctk.set_default_color_theme("blue")  # Themes: "blue", "green", "dark-blue"

# --- GLOBAL STATE ---
app_config = config.load_config()
current_mode = app_config.get("theme", "dark").title() # "Dark" or "Light"
ctk.set_appearance_mode(current_mode)
batch_urls = []


# --- TOOLTIPS WRAPPER (CTK doesn't have native tooltips yet) ---
# We reuse the one from ui_helpers but attach it to CTK widgets
# --- TOOLTIPS FIX ---
def add_tooltip(widget, text):
    try:
        # We access the internal ToolTip class from ui_helpers
        t = ui_helpers.ToolTip(widget, text)
        
        # We monkey-patch the showtip method to use a bigger font
        def custom_showtip(text):
            t.text = text
            if t.tipwindow or not t.text: return
            x, y, cx, cy = t.widget.bbox("insert")
            x = x + t.widget.winfo_rootx() + 25
            y = y + cy + t.widget.winfo_rooty() + 25
            t.tipwindow = tw = tk.Toplevel(t.widget)
            tw.wm_overrideredirect(1)
            tw.wm_geometry("+%d+%d" % (x, y))
            # CHANGED: Increased font size to 11
            label = tk.Label(tw, text=t.text, justify=tk.LEFT,
                           background="#ffffe0", relief=tk.SOLID, borderwidth=1,
                           font=("Segoe UI", 11, "normal"))
            label.pack(ipadx=5, ipady=2)
            
        t.showtip = custom_showtip # Apply patch
        
        def enter(event): t.showtip(text)
        def leave(event): t.hidetip()
        widget.bind('<Enter>', enter)
        widget.bind('<Leave>', leave)
    except: pass
# --- ACTIONS ---
def toggle_theme():
    global current_mode
    new_mode = "Light" if current_mode == "Dark" else "Dark"
    ctk.set_appearance_mode(new_mode)
    current_mode = new_mode
    app_config["theme"] = new_mode.lower()
    config.save_config(app_config)
    btn_theme.configure(text=f"Switch to {('Light' if new_mode == 'Dark' else 'Dark')} Mode")
def update_alert_visibility():
    # Ensure frame_alert exists before trying to pack it
    try:
        if not app_config["ytdlp_path"] or not app_config["ffmpeg_path"]:
            frame_alert.pack(side="top", fill="x")
        else:
            frame_alert.pack_forget()
    except NameError:
        pass # UI hasn't loaded yet

def browse_folder():
    f = filedialog.askdirectory(initialdir=entry_path.get())
    if f:
        entry_path.delete(0, "end")
        entry_path.insert(0, f)

def paste_clipboard():
    try:
        data = app.clipboard_get()
        entry_url.delete(0, "end")
        entry_url.insert(0, data)
    except: pass

def toggle_advanced():
    if var_advanced.get(): 
        frame_advanced_options.pack(fill="x", padx=20, pady=5)
    else: 
        frame_advanced_options.pack_forget()

def load_batch_file():
    fp = filedialog.askopenfilename(filetypes=[("Text Files", "*.txt")])
    if fp:
        with open(fp, "r") as f:
            lines = f.readlines()
            batch_urls.extend(lines)
            lbl_batch_status.configure(text=f"Loaded {len(lines)} URLs", text_color="green")

def update_progress(val):
    # Ensure val is between 0 and 100, then divide for CTK (0.0 to 1.0)
    safe_val = float(val) / 100
    progress_bar.set(safe_val)

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
        lbl_status.configure(text="Error: No URL provided.", text_color="red")
        return

    yt_path = app_config["ytdlp_path"]
    if not os.path.exists(yt_path):
        lbl_status.configure(text="Error: yt-dlp path invalid (Check Settings)", text_color="red")
        return

    # Map friendly names back to config values
    quality_map = {"Best": "Best Possible"} 
    
    options = {
        'yt_path': yt_path,
        'ff_path': app_config["ffmpeg_path"],
        'target_folder': entry_path.get().strip(),
        'mode': var_mode.get().lower(),
        'debug': debug_mode.get(), #yeni
        'is_playlist': var_playlist.get(),
        'custom_tmpl': entry_template.get().strip() if var_advanced.get() else "",
        'use_subs': var_subs.get(),
        'format': combo_vid_format.get(),
        'quality': quality_map.get(combo_quality.get(), combo_quality.get()),
        'audio_fmt': combo_audio.get(),
        'meta_artist': entry_artist.get().strip() if var_metadata.get() else "",
        'meta_album': entry_album.get().strip() if var_metadata.get() else ""
    }

    btn_download.configure(text="CANCEL", fg_color="red", hover_color="darkred", command=cancel_process)
    progress_bar.set(0)
    
    callbacks = {
        'status': lambda msg, col: app.after(0, lambda: lbl_status.configure(text=msg, text_color=col if col != "blue" else ("#1f6aa5" if current_mode=="Light" else "#4da6ff"))),
        'progress': lambda val: app.after(0, lambda: update_progress(val)),
        'refresh_history': lambda: app.after(0, refresh_history_ui),
        'finish': lambda success, msg: app.after(0, lambda: finish_ui_reset(success, msg))
    }

    threading.Thread(target=logic.run_download_logic, args=(all_urls, options, callbacks), daemon=True).start()

def cancel_process():
    if logic.cancel_download():
        lbl_status.configure(text="Cancelling...", text_color="orange")

def finish_ui_reset(success, msg):
    lbl_status.configure(text=msg, text_color="green" if success else "red")
    btn_download.configure(text="EXECUTE DOWNLOAD", fg_color=["#3B8ED0", "#1F6AA5"], hover_color=["#36719F", "#144870"], command=start_download_thread)
    batch_urls.clear()
    lbl_batch_status.configure(text="")
    progress_bar.set(0)
    refresh_history_ui()

# --- HISTORY ---
def refresh_history_ui():
    # CustomTkinter scrollable frame cleanup
    for widget in scroll_history.winfo_children():
        widget.destroy()

    history = config.load_history()

    if not history:
        ctk.CTkLabel(scroll_history, text="No recent downloads.", font=("Arial", 12, "italic"), text_color="gray").pack(pady=20)
        return

    for item in history:
        # Create a Card Frame
        card = ctk.CTkFrame(scroll_history, fg_color=("gray90", "gray20"))
        card.pack(fill="x", padx=5, pady=5)
        
        # Info Column
        info_col = ctk.CTkFrame(card, fg_color="transparent")
        info_col.pack(side="left", fill="both", expand=True, padx=10, pady=5)
        
        # Clickable Title
        title_btn = ctk.CTkButton(info_col, text=item["title"], anchor="w", fg_color="transparent", 
                                  text_color=("black", "white"), hover=False, font=("Arial", 12, "bold"),
                                  command=lambda p=item["path"]: logic.open_file_safe(p))
        title_btn.pack(fill="x")
        
        ctk.CTkLabel(info_col, text=f"{item['duration']}  |  {item['size']}", anchor="w", font=("Arial", 10), text_color="gray").pack(fill="x")

        # Action Buttons Column
        btn_col = ctk.CTkFrame(card, fg_color="transparent")
        btn_col.pack(side="right", padx=10)
        
        ctk.CTkButton(btn_col, text="📂", width=40, height=30, fg_color="transparent", border_width=1, border_color="gray", text_color=("black", "white"),
                      command=lambda p=item["path"]: logic.open_folder_safe(p)).pack(side="left", padx=2)
        
        ctk.CTkButton(btn_col, text="🗑️", width=40, height=30, fg_color="#ffdddd", hover_color="#ffcccc", text_color="red",
                      command=lambda i=item: delete_ui_action(i)).pack(side="left", padx=2)

def delete_ui_action(entry):
    file_path = entry['path']
    file_name = os.path.basename(file_path)
    if messagebox.askyesno("Delete File", f"Permanently delete this file from your disk?\n\n{file_name}"):
        if os.path.exists(file_path):
            try:
                os.remove(file_path)
            except OSError as e:
                messagebox.showerror("Cannot Delete", f"Could not delete the file.\n\nMake sure the video is CLOSED.\n\nError: {e}")
                return 
        hist = config.load_history()
        hist = [x for x in hist if x['path'] != file_path]
        config.save_history_list(hist)
        refresh_history_ui()

def delete_all_action():
    if not config.load_history(): return 
    if messagebox.askyesno("Clear History", "Clear history list?\n(Files stay on disk)"):
        config.save_history_list([])
        refresh_history_ui()

# --- SETTINGS LOGIC ---
def do_autodetect():
    # 1. Detect yt-dlp
    yt = logic.auto_detect_ytdlp()
    if yt: 
        entry_ytdlp.delete(0, "end"); entry_ytdlp.insert(0, yt)
    
    # 2. Detect FFmpeg (Targeting Gyan Shared)
    ff_list = logic.get_all_ffmpeg_paths()
    if ff_list:
        combo_ffmpeg.configure(values=ff_list)
        
        best_choice = None
        
        # LEVEL 1: The "Gold Standard" (Gyan + Shared)
        # This fixes your specific issue by prioritizing the full shared build
        for p in ff_list:
            if "solidworks" in p.lower(): continue
            if "gyan" in p.lower() and "shared" in p.lower():
                best_choice = p
                break
        
        # LEVEL 2: Fallback to any Gyan or Full Build if Shared not found
        if not best_choice:
            for p in ff_list:
                if "solidworks" in p.lower(): continue
                if "gyan" in p.lower() or "full" in p.lower():
                    best_choice = p
                    break
                    
        # LEVEL 3: Take whatever is left (Winget/Bin), excluding SolidWorks
        if not best_choice:
            for p in ff_list:
                if "solidworks" not in p.lower():
                    best_choice = p
                    break
        
        # Final Fallback
        if not best_choice: 
            best_choice = ff_list[0]

        combo_ffmpeg.set(best_choice)
        messagebox.showinfo("Success", f"Found {len(ff_list)} paths.\nSelected: {os.path.basename(os.path.dirname(best_choice))}")
    else: 
        messagebox.showinfo("Result", "yt-dlp found, but FFmpeg missing.")
        
def do_save_settings():
    app_config["ytdlp_path"] = entry_ytdlp.get().strip()
    app_config["ffmpeg_path"] = combo_ffmpeg.get().strip()
    config.save_config(app_config)
    show_main()

def show_settings():
    frame_main.pack_forget()
    frame_settings.pack(fill="both", expand=True)

    # Clear previous widgets
    for widget in frame_settings.winfo_children():
        widget.destroy()

    ctk.CTkLabel(frame_settings, text="First Time Setup", font=("Arial", 22, "bold")).pack(pady=(20, 10))

    # --- BEGINNER INSTRUCTIONS ---
    guide_text = (
        "STEP 1: Click 'Install yt-dlp' and 'Install FFmpeg' below.\n"
        "             (Wait for the black console windows to close)\n\n"
        "STEP 2: Click 'Auto-Detect Paths' to find the installed tools.\n\n"
        "STEP 3: If paths appear in the boxes, click 'Save & Return'.\n"
        "If multiple FFmpeg is Found select the one starting With 'Gyan'"
        
    )
    
    lbl_guide = ctk.CTkLabel(frame_settings, text=guide_text, text_color="gray70", 
                             justify="left", font=("Consolas", 12),
                             fg_color=("gray90", "gray20"), corner_radius=6)
    lbl_guide.pack(padx=20, pady=10, ipadx=10, ipady=10)
    # -----------------------------

    # Input Fields
    fs_yt = ctk.CTkFrame(frame_settings, fg_color="transparent")
    fs_yt.pack(fill="x", padx=40, pady=5)
    ctk.CTkLabel(fs_yt, text="yt-dlp:", width=80, anchor="w").pack(side="left")
    global entry_ytdlp 
    entry_ytdlp = ctk.CTkEntry(fs_yt)
    entry_ytdlp.insert(0, app_config["ytdlp_path"])
    entry_ytdlp.pack(side="left", fill="x", expand=True)

    fs_ff = ctk.CTkFrame(frame_settings, fg_color="transparent")
    fs_ff.pack(fill="x", padx=40, pady=5)
    ctk.CTkLabel(fs_ff, text="FFmpeg:", width=80, anchor="w").pack(side="left")
    global combo_ffmpeg
    combo_ffmpeg = ctk.CTkComboBox(fs_ff, values=[app_config["ffmpeg_path"]])
    combo_ffmpeg.set(app_config["ffmpeg_path"])
    combo_ffmpeg.pack(side="left", fill="x", expand=True)

    # Install Buttons
    fs_inst = ctk.CTkFrame(frame_settings, fg_color="transparent")
    fs_inst.pack(pady=10)
    ctk.CTkButton(fs_inst, text="⬇ Install yt-dlp", width=140, command=lambda: logic.install_via_winget("yt-dlp")).pack(side="left", padx=10)
    ctk.CTkButton(fs_inst, text="⬇ Install FFmpeg", width=140, command=lambda: logic.install_via_winget("Gyan.FFmpeg")).pack(side="left", padx=10)

    # Action Buttons
    ctk.CTkButton(frame_settings, text="🔍 Auto-Detect Paths", width=200, fg_color="#3B8ED0", command=do_autodetect).pack(pady=10)
    
    ctk.CTkButton(frame_settings, text="✅ Save & Return", width=200, height=40, fg_color="green", hover_color="darkgreen", command=do_save_settings).pack(pady=(20, 10))
    ctk.CTkButton(frame_settings, text="Cancel", width=200, fg_color="transparent", border_width=1, text_color=("black", "white"), command=show_main).pack()
    
    # Factory Reset
    ctk.CTkButton(frame_settings, text="Factory Reset", fg_color="transparent", text_color="red", hover_color="#ffdddd", command=do_reset).pack(side="bottom", pady=20)


def show_main():
    frame_settings.pack_forget()
    frame_main.pack(fill="both", expand=True)
    app_config.update(config.load_config())
    update_alert_visibility()

def do_reset():
    if messagebox.askyesno("Reset", "Delete all settings?"):
        config.factory_reset(); app.destroy(); os._exit(0)

# ================= LAYOUT =================
# ================= LAYOUT =================
app = ctk.CTk()
app.title(f"yt-mini v{config.VERSION}")

# 1. Set the Start Size
app.geometry("570x620")

# 2. Set the Minimum Size (User cannot shrink below this)
app.minsize(570, 620)
debug_mode = ctk.BooleanVar(value=False) # New global variable yeni

# --- MAIN FRAME ---
frame_main = ctk.CTkFrame(app, fg_color="transparent")
frame_main.pack(fill="both", expand=True)

# --- ALERT FRAME (Hidden by default) ---
frame_alert = ctk.CTkFrame(frame_main, fg_color="transparent")
btn_alert = ctk.CTkButton(frame_alert, text="⚠️ SETUP REQUIRED: Begin the setup here", 
                          fg_color="#d9534f", hover_color="#c9302c", 
                          height=40, font=("Arial", 12, "bold"),
                          command=show_settings)
btn_alert.pack(fill="x", padx=20, pady=(10, 0))
# ---------------------------------------
# ... rest of the layout code ...

# Header
frame_top = ctk.CTkFrame(frame_main, fg_color="transparent")
frame_top.pack(side="top", fill="x", pady=(10, 0))
header = ctk.CTkFrame(frame_top, fg_color="transparent")
header.pack(fill="x", padx=20)
btn_settings = ctk.CTkButton(header, text="⚙️ Setup", width=80, height=28, command=show_settings)
btn_settings.pack(side="left")
btn_theme = ctk.CTkButton(header, text="Switch Theme", width=100, height=28, fg_color="gray", command=toggle_theme)
btn_theme.pack(side="right")

# URL Input
url_cont = ctk.CTkFrame(frame_top, fg_color="transparent")
url_cont.pack(pady=(10, 5))
ctk.CTkLabel(url_cont, text="Paste Link:", anchor="w").pack(fill="x", padx=250)
frame_u = ctk.CTkFrame(url_cont, fg_color="transparent")
frame_u.pack(fill="x", padx=20)
# CHANGED: width increased and font added
entry_url = ctk.CTkEntry(frame_u, placeholder_text="https://youtube.com/watch?v=...", height=30, font=("Segoe UI", 12))
entry_url.pack(side="left", fill="x", expand=True, padx=(0, 5))
btn_p = ctk.CTkButton(frame_u, text="📋", width=40, height=35, command=paste_clipboard)
btn_p.pack(side="left")
add_tooltip(btn_p, config.TOOLTIPS["paste"])

# Path Input
path_cont = ctk.CTkFrame(frame_top, fg_color="transparent")
path_cont.pack(pady=5)
ctk.CTkLabel(path_cont, text="Save to:", anchor="w").pack(fill="x", padx=240)
frame_p = ctk.CTkFrame(path_cont, fg_color="transparent")
frame_p.pack(fill="x", padx=20)
# CHANGED: width increased
entry_path = ctk.CTkEntry(frame_p, height=30, font=("Segoe UI", 12))
entry_path.insert(0, app_config["download_path"])
entry_path.pack(side="left", fill="x", expand=True, padx=(0, 5))
btn_b = ctk.CTkButton(frame_p, text="Browse", width=80, height=30, command=browse_folder)
btn_b.pack(side="left")
add_tooltip(btn_b, config.TOOLTIPS["browse"])

# Checkboxes
opt_cont = ctk.CTkFrame(frame_top, fg_color="transparent")
opt_cont.pack(pady=5, padx=20, fill="x")
var_playlist = ctk.BooleanVar()
chk_pl = ctk.CTkCheckBox(opt_cont, text="Playlist (Auto-Create Folder)", variable=var_playlist)
chk_pl.pack(side="left", padx=(0, 10))
add_tooltip(chk_pl, config.TOOLTIPS["playlist"])
var_subs = ctk.BooleanVar()
chk_s = ctk.CTkCheckBox(opt_cont, text="Download Subtitles", variable=var_subs)
chk_s.pack(side="left")
add_tooltip(chk_s, config.TOOLTIPS["subs"])

# Advanced Toggle
var_advanced = ctk.BooleanVar()
chk_adv = ctk.CTkCheckBox(frame_top, text="Show Advanced Options", variable=var_advanced, command=toggle_advanced, text_color=("blue", "#4da6ff"))
chk_adv.pack(pady=5, padx=20, anchor="w")
add_tooltip(chk_adv, config.TOOLTIPS["advanced"])

# Advanced Options (Hidden)
frame_advanced_options = ctk.CTkFrame(frame_top, fg_color=("gray85", "gray25"))
ctk.CTkLabel(frame_advanced_options, text="Filename Template:", font=("Arial", 11)).pack(anchor="w", padx=10, pady=(5,0))
entry_template = ctk.CTkEntry(frame_advanced_options, placeholder_text="%(title)s.%(ext)s")
entry_template.pack(fill="x", padx=10, pady=5)
ctk.CTkButton(frame_advanced_options, text="Import .txt (Batch)", command=load_batch_file).pack(pady=5)
lbl_batch_status = ctk.CTkLabel(frame_advanced_options, text="", text_color="green")
lbl_batch_status.pack()

# Mode Selection
var_mode = ctk.StringVar(value="video")
mode_f = ctk.CTkSegmentedButton(frame_top, values=["Video", "Sound"], variable=var_mode, command=lambda v: update_vis())
mode_f.pack(pady=10)
mode_f.set("Video")

def update_vis():
    if var_mode.get() == "Video": 
        audio_c.pack_forget()
        video_c.pack(pady=5, padx=20, fill="x")
    else: 
        video_c.pack_forget()
        audio_c.pack(pady=5, padx=20, fill="x")

# Video Config
video_c = ctk.CTkFrame(frame_top, fg_color="transparent")
ctk.CTkLabel(video_c, text="Format:").pack(side="left", padx=5)
combo_vid_format = ctk.CTkOptionMenu(video_c, values=["WebM (VP9)", "MP4 (H264)"], width=130)
combo_vid_format.pack(side="left", padx=5)
ctk.CTkLabel(video_c, text="Quality:").pack(side="left", padx=5)
# Added "1080" to the values list
combo_quality = ctk.CTkOptionMenu(video_c, values=["144", "240", "360", "720", "1080", "1440", "2k", "4k", "Best"], width=90)
combo_quality.pack(side="left", padx=5)
combo_quality.set("720")

# Audio Config
audio_c = ctk.CTkFrame(frame_top, fg_color="transparent")
audio_options = [
    "MP3 - High (~320kbps)", "MP3 - Medium (~128kbps)", "MP3 - Low (~64kbps)", 
    "Opus - High (~160kbps)", "Opus - Medium (~96kbps)", "Opus - Low (~48kbps)", 
    "AAC - High (~256kbps)", "AAC - Medium (~128kbps)", "AAC - Low (~64kbps)"
]
combo_audio = ctk.CTkOptionMenu(audio_c, values=audio_options, width=200)
combo_audio.pack(pady=5)
combo_audio.set("MP3 - High (~320kbps)")

frame_m = ctk.CTkFrame(audio_c, fg_color="transparent")
frame_m.pack(pady=5)
var_metadata = ctk.BooleanVar()
ctk.CTkCheckBox(frame_m, text="Tag Metadata", variable=var_metadata).grid(row=0, column=0, columnspan=2, sticky="w", pady=5)
ctk.CTkLabel(frame_m, text="Artist:").grid(row=1, column=0, padx=5); entry_artist = ctk.CTkEntry(frame_m, width=120); entry_artist.grid(row=1, column=1)
ctk.CTkLabel(frame_m, text="Album:").grid(row=2, column=0, padx=5); entry_album = ctk.CTkEntry(frame_m, width=120); entry_album.grid(row=2, column=1)

# Footer
progress_bar = ctk.CTkProgressBar(frame_main, orientation="horizontal", height=10)
progress_bar.pack(side="bottom", fill="x", padx=20, pady=(0, 10))
progress_bar.set(0)

lbl_status = ctk.CTkLabel(frame_main, text="Ready")
lbl_status.pack(side="bottom", pady=5)

btn_download = ctk.CTkButton(frame_main, text="EXECUTE DOWNLOAD", height=45, font=("Arial", 14, "bold"), command=start_download_thread)
btn_download.pack(side="bottom", pady=10)

# History List
hist_head = ctk.CTkFrame(frame_main, fg_color="transparent")
hist_head.pack(side="top", fill="x", padx=20, pady=(10, 5))
ctk.CTkLabel(hist_head, text="Recent Downloads", font=("Arial", 14, "bold")).pack(side="left")
ctk.CTkButton(hist_head, text="Clear History", width=80, fg_color="transparent", border_width=1, border_color="red", text_color="red", hover_color="#ffdddd", command=delete_all_action).pack(side="right")

scroll_history = ctk.CTkScrollableFrame(frame_main, label_text="")
scroll_history.pack(side="top", fill="both", expand=True, padx=20, pady=(0, 10))

# --- SETTINGS SCREEN ---
frame_settings = ctk.CTkFrame(app, fg_color="transparent")
ctk.CTkLabel(frame_settings, text="Settings & Setup", font=("Arial", 20, "bold")).pack(pady=30)
ctk.CTkLabel(frame_settings, text="Use 'Auto-Detect' to find installed tools.", text_color="gray").pack()

fs_yt = ctk.CTkFrame(frame_settings, fg_color="transparent"); fs_yt.pack(fill="x", padx=40, pady=10)
ctk.CTkLabel(fs_yt, text="yt-dlp:", width=80, anchor="w").pack(side="left")
entry_ytdlp = ctk.CTkEntry(fs_yt); entry_ytdlp.insert(0, app_config["ytdlp_path"]); entry_ytdlp.pack(side="left", fill="x", expand=True)

fs_ff = ctk.CTkFrame(frame_settings, fg_color="transparent"); fs_ff.pack(fill="x", padx=40, pady=10)
ctk.CTkLabel(fs_ff, text="FFmpeg:", width=80, anchor="w").pack(side="left")
combo_ffmpeg = ctk.CTkComboBox(fs_ff, values=[app_config["ffmpeg_path"]]); combo_ffmpeg.set(app_config["ffmpeg_path"]); combo_ffmpeg.pack(side="left", fill="x", expand=True)

fs_inst = ctk.CTkFrame(frame_settings, fg_color="transparent"); fs_inst.pack(pady=20)
ctk.CTkButton(fs_inst, text="Install yt-dlp", command=lambda: logic.install_via_winget("yt-dlp")).pack(side="left", padx=10)
ctk.CTkButton(fs_inst, text="Install FFmpeg", command=lambda: logic.install_via_winget("Gyan.FFmpeg")).pack(side="left", padx=10)

ctk.CTkButton(frame_settings, text="Auto-Detect Paths", width=200, command=do_autodetect).pack(pady=10)
ctk.CTkButton(frame_settings, text="Check Updates", width=200, fg_color="gray", command=lambda: logic.update_tools(entry_ytdlp.get())).pack(pady=10)

ctk.CTkButton(frame_settings, text="Save & Return", width=200, height=40, fg_color="green", hover_color="darkgreen", command=do_save_settings).pack(pady=30)
ctk.CTkButton(frame_settings, text="Cancel", width=200, fg_color="transparent", border_width=1, text_color=("black", "white"), command=show_main).pack()
ctk.CTkButton(frame_settings, text="⚠️ Factory Reset", fg_color="transparent", text_color="red", hover_color="#ffdddd", command=do_reset).pack(side="bottom", pady=20)

# Init
update_vis()
refresh_history_ui()
update_alert_visibility()
app.mainloop()