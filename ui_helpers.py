import tkinter as tk
import config
import os

# --- TOOLTIPS ---
class ToolTip(object):
    def __init__(self, widget, text):
        self.widget = widget
        self.text = text
        self.tipwindow = None
    def showtip(self, text):
        self.text = text
        if self.tipwindow or not self.text: return
        x, y, cx, cy = self.widget.bbox("insert")
        x = x + self.widget.winfo_rootx() + 25
        y = y + cy + self.widget.winfo_rooty() + 25
        self.tipwindow = tw = tk.Toplevel(self.widget)
        tw.wm_overrideredirect(1)
        tw.wm_geometry("+%d+%d" % (x, y))
        label = tk.Label(tw, text=self.text, justify=tk.LEFT,
                       background="#ffffe0", relief=tk.SOLID, borderwidth=1,
                       font=("tahoma", "8", "normal"))
        label.pack(ipadx=1)
    def hidetip(self):
        tw = self.tipwindow
        self.tipwindow = None
        if tw: tw.destroy()

def create_tooltip(widget, text):
    toolTip = ToolTip(widget, text)
    def enter(event): toolTip.showtip(text)
    def leave(event): toolTip.hidetip()
    widget.bind('<Enter>', enter)
    widget.bind('<Leave>', leave)

# --- THEME ENGINE ---
def recursive_theme_update(widget, colors):
    try:
        w_type = widget.winfo_class()
        if w_type == 'Label': widget.configure(bg=colors["bg"], fg=colors["fg"])
        elif w_type == 'Button': 
            txt = widget.cget('text')
            if txt not in ["🗑️", "⚠️ Factory Reset", "Save & Return", "CANCEL"]:
                widget.configure(bg=colors["btn_bg"], fg=colors["btn_fg"], activebackground=colors["btn_bg"])
        elif w_type == 'Entry': widget.configure(bg=colors["entry_bg"], fg=colors["entry_fg"], insertbackground=colors["fg"])
        elif w_type == 'Checkbutton' or w_type == 'Radiobutton': widget.configure(bg=colors["bg"], fg=colors["fg"], selectcolor=colors["bg"], activebackground=colors["bg"])
        elif w_type == 'Frame' or w_type == 'Labelframe' or w_type == 'Canvas': widget.configure(bg=colors["bg"])
        
        for child in widget.winfo_children():
            recursive_theme_update(child, colors)
    except: pass

def apply_theme(root, current_theme_name):
    colors = config.THEMES[current_theme_name]
    root.configure(bg=colors["bg"])
    recursive_theme_update(root, colors)
    return colors

# --- HISTORY CARD CREATOR ---
def create_history_card(parent_frame, item, colors, open_file_cmd, open_folder_cmd, delete_cmd):
    card = tk.Frame(parent_frame, bg=colors["card_bg"], highlightbackground=colors["card_border"], highlightthickness=1)
    card.pack(fill="x", padx=10, pady=5, ipady=5)

    info_frame = tk.Frame(card, bg=colors["card_bg"])
    info_frame.pack(side="left", fill="both", expand=True, padx=10)

    # Title Button -> Opens FILE
    tk.Button(info_frame, text=item["title"], anchor="w", font=("Arial", 9, "bold"),
              bg=colors["card_bg"], fg=colors["fg"], borderwidth=0, activebackground=colors["card_bg"],
              command=lambda: open_file_cmd(item["path"])).pack(fill="x")
    
    # Details
    tk.Label(info_frame, text=f"{item['duration']}  |  {item['size']}", anchor="w", font=("Arial", 8), 
             bg=colors["card_bg"], fg=colors["status_fg"]).pack(fill="x")

    btn_frame = tk.Frame(card, bg=colors["card_bg"])
    btn_frame.pack(side="right", padx=5)

    # Folder Button -> Opens FOLDER
    tk.Button(btn_frame, text="📂", width=3, bg=colors["btn_bg"], fg=colors["btn_fg"],
              command=lambda: open_folder_cmd(item["path"])).pack(side="left", padx=2)
    
    # Delete Button
    tk.Button(btn_frame, text="🗑️", width=3, bg="#ffdddd", fg="red",
              command=lambda: delete_cmd(item)).pack(side="left", padx=2)