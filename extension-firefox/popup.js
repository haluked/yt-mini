const BRIDGE_URL = "http://127.0.0.1:48123";

let currentTabUrl = "";
let currentMediaData = null;
let selectedFormat = "MP4 (H.264)";
let selectedMode = "video";
let activeJobPolling = null;

// UI Elements
const statusPill = document.getElementById("status-pill");
const statusText = document.getElementById("status-text");
const offlineBanner = document.getElementById("offline-banner");
const btnAutostart = document.getElementById("btn-autostart");
const mediaCard = document.getElementById("media-card");
const mediaThumb = document.getElementById("media-thumb");
const mediaDuration = document.getElementById("media-duration");
const mediaTitle = document.getElementById("media-title");
const mediaUploader = document.getElementById("media-uploader");
const playlistBadge = document.getElementById("playlist-badge");
const noMediaCard = document.getElementById("no-media-card");
const formatSection = document.getElementById("format-section");
const btnDownloadNow = document.getElementById("btn-download-now");
const progressCard = document.getElementById("progress-card");
const progTitle = document.getElementById("prog-title");
const progPct = document.getElementById("prog-pct");
const progressBarFill = document.getElementById("progress-bar-fill");
const progSpeed = document.getElementById("prog-speed");
const progEta = document.getElementById("prog-eta");
const completeCard = document.getElementById("complete-card");
const completeFilename = document.getElementById("complete-filename");
const btnOpenDownloads = document.getElementById("btn-open-downloads");
const inputCustomUrl = document.getElementById("input-custom-url");
const btnCustomDownload = document.getElementById("btn-custom-download");
const toast = document.getElementById("toast");

document.addEventListener("DOMContentLoaded", async () => {
  setupFormatPills();
  setupCustomUrl();
  setupAutoStart();

  await initConnectionAndInspect();
});

// Check bridge and inspect active tab
async function initConnectionAndInspect() {
  const isBridgeAlive = await checkBridgeConnection();
  if (!isBridgeAlive) {
    showOfflineState();
    return;
  }

  hideOfflineState();

  chrome.tabs.query({ active: true, currentWindow: true }, async (tabs) => {
    if (!tabs || tabs.length === 0) {
      showNoMediaState();
      return;
    }

    const activeTab = tabs[0];
    currentTabUrl = activeTab.url || "";

    // Check cached tab data first
    chrome.runtime.sendMessage({ action: "getCachedMedia", tabId: activeTab.id }, async (response) => {
      if (response && response.media && response.media.downloadable) {
        showMediaReady(response.media);
      } else {
        await inspectActiveUrl(currentTabUrl);
      }
    });
  });
}

// Ping bridge server
async function checkBridgeConnection() {
  try {
    const controller = new AbortController();
    const id = setTimeout(() => controller.abort(), 1200);
    const resp = await fetch(`${BRIDGE_URL}/status`, { signal: controller.signal });
    clearTimeout(id);
    return resp.ok;
  } catch (e) {
    return false;
  }
}

// Inspect active URL without downloading media stream
async function inspectActiveUrl(url) {
  if (!url || url.startsWith("chrome://") || url.startsWith("brave://") || url.startsWith("edge://") || url.startsWith("about:")) {
    showNoMediaState();
    return;
  }

  statusPill.className = "status-pill status-checking";
  statusText.textContent = "Inspecting...";

  try {
    const controller = new AbortController();
    const id = setTimeout(() => controller.abort(), 4000);
    const resp = await fetch(`${BRIDGE_URL}/inspect`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ url }),
      signal: controller.signal
    });
    clearTimeout(id);

    if (resp.ok) {
      const data = await resp.json();
      if (data && data.downloadable) {
        showMediaReady(data);
        return;
      }
    }
  } catch (e) {}

  showNoMediaState();
}

function showMediaReady(data) {
  currentMediaData = data;
  offlineBanner.classList.add("hidden");
  noMediaCard.classList.add("hidden");

  // Green Indicator Light
  statusPill.className = "status-pill status-ready";
  statusText.textContent = "Media Ready";

  mediaTitle.textContent = data.title || "Untitled Video";
  mediaUploader.textContent = data.uploader ? `By ${data.uploader}` : "";
  mediaDuration.textContent = data.duration || "";

  if (data.thumbnail_url) {
    mediaThumb.src = data.thumbnail_url;
    mediaThumb.style.display = "block";
  } else {
    mediaThumb.style.display = "none";
  }

  if (data.is_playlist) {
    playlistBadge.textContent = `📁 Playlist • ${data.playlist_count} Tracks`;
    playlistBadge.classList.remove("hidden");
  } else {
    playlistBadge.classList.add("hidden");
  }

  mediaCard.classList.remove("hidden");
  formatSection.classList.remove("hidden");
  btnDownloadNow.classList.remove("hidden");
}

function showNoMediaState() {
  statusPill.className = "status-pill";
  statusText.textContent = "No Media";
  mediaCard.classList.add("hidden");
  formatSection.classList.add("hidden");
  btnDownloadNow.classList.add("hidden");
  noMediaCard.classList.remove("hidden");
}

function showOfflineState() {
  statusPill.className = "status-pill status-offline";
  statusText.textContent = "App Offline";
  offlineBanner.classList.remove("hidden");
  mediaCard.classList.add("hidden");
  formatSection.classList.add("hidden");
  btnDownloadNow.classList.add("hidden");
  noMediaCard.classList.add("hidden");
}

function hideOfflineState() {
  offlineBanner.classList.add("hidden");
}

// 1-Click Auto-Start via ytmini:// custom protocol
function setupAutoStart() {
  btnAutostart.addEventListener("click", () => {
    btnAutostart.textContent = "⏳ Launching...";
    // Trigger custom protocol
    window.location.href = "ytmini://launch";

    // Poll for bridge to come online
    let attempts = 0;
    const interval = setInterval(async () => {
      attempts++;
      const alive = await checkBridgeConnection();
      if (alive) {
        clearInterval(interval);
        btnAutostart.textContent = "🚀 Auto-Start yt-mini";
        await initConnectionAndInspect();
      } else if (attempts >= 8) {
        clearInterval(interval);
        btnAutostart.textContent = "🚀 Retry Launch";
        showToast("Could not start automatically. Launch main.py manually.");
      }
    }, 700);
  });
}

// Format selection pills
function setupFormatPills() {
  const pills = document.querySelectorAll(".pill");
  pills.forEach((p) => {
    p.addEventListener("click", () => {
      pills.forEach((btn) => btn.classList.remove("active"));
      p.classList.add("active");
      selectedFormat = p.dataset.format;
      selectedMode = p.dataset.mode;
    });
  });

  btnDownloadNow.addEventListener("click", () => {
    if (!currentTabUrl) return;
    startDownloadFlow(currentTabUrl, selectedFormat, selectedMode);
  });

  btnOpenDownloads.addEventListener("click", () => {
    const isFirefox = typeof InstallTrigger !== 'undefined' || navigator.userAgent.toLowerCase().includes("firefox");
    const downloadPage = isFirefox ? "about:downloads" : "chrome://downloads";
    chrome.tabs.create({ url: downloadPage });
  });
}

// Start download and stream progress
async function startDownloadFlow(url, format, mode) {
  btnDownloadNow.classList.add("hidden");
  formatSection.classList.add("hidden");
  mediaCard.classList.add("hidden");
  progressCard.classList.remove("hidden");
  completeCard.classList.add("hidden");

  progTitle.textContent = "⚡ Initializing Download...";
  progPct.textContent = "0%";
  progressBarFill.style.width = "0%";
  progSpeed.textContent = "Connecting to media stream...";
  progEta.textContent = "";

  try {
    const resp = await fetch(`${BRIDGE_URL}/download`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        url: url,
        format: format,
        mode: mode,
        is_playlist: currentMediaData ? currentMediaData.is_playlist : false
      })
    });

    if (!resp.ok) {
      throw new Error("Server rejected download");
    }

    const resData = await resp.json();
    const jobId = resData.job_id;

    if (!jobId) {
      showToast("Download dispatched to yt-mini desktop!");
      return;
    }

    // Begin real-time progress polling
    pollJobProgress(jobId);

  } catch (err) {
    progressCard.classList.add("hidden");
    btnDownloadNow.classList.remove("hidden");
    formatSection.classList.remove("hidden");
    mediaCard.classList.remove("hidden");
    showToast("Failed to connect to yt-mini.");
  }
}

// Poll job progress every 400ms
function pollJobProgress(jobId) {
  if (activeJobPolling) clearInterval(activeJobPolling);

  activeJobPolling = setInterval(async () => {
    try {
      const resp = await fetch(`${BRIDGE_URL}/job_status?id=${jobId}`);
      if (!resp.ok) return;

      const job = await resp.json();

      if (job.status === "downloading" || job.status === "initializing") {
        const pct = Math.max(0, Math.min(100, Math.round(job.percent || 0)));
        progPct.textContent = `${pct}%`;
        progressBarFill.style.width = `${pct}%`;
        progTitle.textContent = pct > 98 ? "⚡ Merging Audio & Video (FFmpeg)..." : "⚡ Downloading Media...";
        progSpeed.textContent = job.speed ? `⚡ ${job.speed}` : "Transcoding stream...";
        progEta.textContent = job.eta ? `⏳ ETA ${job.eta}` : "";
      } else if (job.status === "complete") {
        clearInterval(activeJobPolling);
        onJobFinished(jobId, job);
      } else if (job.status === "error") {
        clearInterval(activeJobPolling);
        progressCard.classList.add("hidden");
        btnDownloadNow.classList.remove("hidden");
        formatSection.classList.remove("hidden");
        mediaCard.classList.remove("hidden");
        showToast(`Download failed: ${job.error || "Unknown error"}`);
      }
    } catch (e) {}
  }, 400);
}

// Trigger native browser download manager (Ctrl + J)
function onJobFinished(jobId, job) {
  progressCard.classList.add("hidden");
  completeCard.classList.remove("hidden");

  const finalName = job.filename || "downloaded_media.mp4";
  completeFilename.textContent = finalName;

  // Trigger Chrome/Brave native download manager so it appears in Ctrl + J
  try {
    const fileServeUrl = `${BRIDGE_URL}/serve?id=${jobId}`;
    chrome.downloads.download({
      url: fileServeUrl,
      filename: finalName,
      conflictAction: "uniquify"
    }, (downloadId) => {
      console.log("Registered in browser download manager with id:", downloadId);
    });
  } catch (e) {
    console.error("Error triggering browser download:", e);
  }
}

// Custom URL input
function setupCustomUrl() {
  btnCustomDownload.addEventListener("click", () => {
    const url = inputCustomUrl.value.trim();
    if (url) {
      startDownloadFlow(url, selectedFormat, selectedMode);
      inputCustomUrl.value = "";
    }
  });

  inputCustomUrl.addEventListener("keypress", (e) => {
    if (e.key === "Enter") {
      const url = inputCustomUrl.value.trim();
      if (url) {
        startDownloadFlow(url, selectedFormat, selectedMode);
        inputCustomUrl.value = "";
      }
    }
  });
}

function showToast(msg) {
  toast.textContent = msg;
  toast.classList.remove("hidden");
  setTimeout(() => toast.classList.add("hidden"), 3000);
}
