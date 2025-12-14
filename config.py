import os
import json
import logging

VERSION = "3.0"
CONFIG_FILE = "config.txt"
HISTORY_FILE = "history.json"
LOG_FILE = "debug.log"

CREATE_NO_WINDOW = 0x08000000 if os.name == 'nt' else 0

# Tooltips
TOOLTIPS = {
    "paste": "Paste URL from clipboard",
    "browse": "Select download folder",
    "playlist": "If checked, creates a subfolder named after the playlist.\nFiles will be numbered (01 - Title).",
    "subs": "Downloads English and Original language subtitles (.vtt)",
    "advanced": "Reveal Batch Mode and custom filename templates",
    "format": "WebM (VP9): Better compression (smaller files).\nMP4 (H264): Better compatibility (plays everywhere).",
    "quality": "Downloads the best available quality up to this limit.",
    "audio_fmt": "Select the codec and bitrate.\nOpus is most efficient, MP3 is most compatible.",
    "meta": "Embed Artist/Album tags directly into the music file."
}

# Logging
logging.basicConfig(
    filename=LOG_FILE,
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)

DEFAULT_CONFIG = {
    "ytdlp_path": "",
    "ffmpeg_path": "",
    "download_path": os.path.join(os.path.expanduser("~"), "Desktop"),
    "theme": "dark"
}

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
        except Exception as e:
            logging.error(f"Config Load Error: {e}")
    return config

def save_config(config_data):
    try:
        with open(CONFIG_FILE, "w") as f:
            for key, value in config_data.items():
                f.write(f"{key}={value}\n")
    except Exception as e:
        logging.error(f"Config Save Error: {e}")

def load_history():
    if os.path.exists(HISTORY_FILE):
        try:
            with open(HISTORY_FILE, "r") as f:
                return json.load(f)
        except Exception as e:
            logging.error(f"History Load Error: {e}")
            return []
    return []

# --- THIS WAS THE MISSING FUNCTION ---
def save_history_list(history_list):
    try:
        temp_file = HISTORY_FILE + ".tmp"
        with open(temp_file, "w") as f:
            json.dump(history_list, f, indent=4)
        if os.path.exists(HISTORY_FILE):
            os.remove(HISTORY_FILE)
        os.rename(temp_file, HISTORY_FILE)
    except Exception as e:
        logging.error(f"History Save Error: {e}")
# -------------------------------------

def add_to_history(entry):
    history = load_history()
    history = [x for x in history if x['path'] != entry['path']]
    history.insert(0, entry)
    history = history[:50]
    save_history_list(history) # Now reuses the safe save function

def factory_reset():
    try:
        if os.path.exists(CONFIG_FILE): os.remove(CONFIG_FILE)
        if os.path.exists(HISTORY_FILE): os.remove(HISTORY_FILE)
        if os.path.exists(LOG_FILE): os.remove(LOG_FILE)
    except Exception as e:
        logging.error(f"Reset Error: {e}")