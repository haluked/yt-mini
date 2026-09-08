import os
import json
import subprocess
import logging

APP_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG_FILE = os.path.join(APP_DIR, "config.json")
HISTORY_FILE = os.path.join(APP_DIR, "history.json")
LOG_FILE = os.path.join(APP_DIR, "yt_mini.log")

logging.basicConfig(
    filename=LOG_FILE,
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)

# Standard default download folder (Downloads or Desktop)
_default_download = os.path.join(os.path.expanduser("~"), "Downloads")
if not os.path.exists(_default_download):
    _default_download = os.path.join(os.path.expanduser("~"), "Desktop")

DEFAULT_CONFIG = {
    "download_path": _default_download,
    "theme": "Dark",
    "default_format": "MP4",
    "video_quality": "1080p",
    "audio_quality": "320 kbps",
    "auto_playlist_folder": True,
    "enumerate_playlist": True,
    "download_subtitles": False,
    "embed_metadata": True,
    "ffmpeg_path": ""
}

def load_config() -> dict:
    """Load configuration from config.json, merged with defaults."""
    config = DEFAULT_CONFIG.copy()
    if os.path.exists(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, "r", encoding="utf-8") as f:
                saved = json.load(f)
                if isinstance(saved, dict):
                    config.update(saved)
        except Exception as e:
            logging.error(f"Error loading config: {e}")
    return config

def save_config(config_data: dict):
    """Save configuration to config.json atomically."""
    try:
        temp_file = CONFIG_FILE + ".tmp"
        with open(temp_file, "w", encoding="utf-8") as f:
            json.dump(config_data, f, indent=4, ensure_ascii=False)
        os.replace(temp_file, CONFIG_FILE)
    except Exception as e:
        logging.error(f"Error saving config: {e}")

def load_history() -> list:
    """Load recent downloads history."""
    if os.path.exists(HISTORY_FILE):
        try:
            with open(HISTORY_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
                if isinstance(data, list):
                    return data
        except Exception as e:
            logging.error(f"Error loading history: {e}")
    return []

def save_history(history_list: list):
    """Save recent downloads history atomically."""
    try:
        temp_file = HISTORY_FILE + ".tmp"
        with open(temp_file, "w", encoding="utf-8") as f:
            json.dump(history_list, f, indent=4, ensure_ascii=False)
        os.replace(temp_file, HISTORY_FILE)
    except Exception as e:
        logging.error(f"Error saving history: {e}")

def add_history_entry(entry: dict):
    """Add or update an entry at top of history, capping at 100 items."""
    history = load_history()
    # Deduplicate by path
    history = [item for item in history if item.get("path") != entry.get("path")]
    history.insert(0, entry)
    history = history[:100]
    save_history(history)

def remove_history_entry(file_path: str):
    """Remove an entry matching the given file path."""
    history = load_history()
    history = [item for item in history if item.get("path") != file_path]
    save_history(history)

def clear_history():
    """Clear all history items."""
    save_history([])

def open_file_safely(file_path: str) -> tuple[bool, str]:
    """Open a media file using the default OS application."""
    file_path = os.path.normpath(str(file_path).strip())
    if os.path.exists(file_path):
        try:
            os.startfile(file_path)
            return True, "Opened successfully"
        except Exception as e:
            return False, f"Failed to open file: {e}"
    return False, "File does not exist (it may have been moved or deleted)."

def open_folder_safely(file_path: str) -> tuple[bool, str]:
    """Open Explorer and select the file, or open containing folder."""
    file_path = os.path.normpath(str(file_path).strip())
    if os.path.exists(file_path):
        try:
            subprocess.run(["explorer", f'/select,"{file_path}"'], check=False)
            return True, "Folder opened"
        except Exception as e:
            return False, f"Failed to open Explorer: {e}"
    
    # Fallback to parent directory if file was moved/deleted
    folder = os.path.dirname(file_path)
    if os.path.exists(folder):
        try:
            os.startfile(folder)
            return True, "Containing folder opened"
        except Exception as e:
            return False, f"Failed to open folder: {e}"
    return False, "Folder does not exist."
