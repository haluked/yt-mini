const BRIDGE_URL = "http://127.0.0.1:48123";

// Cache for recent tab inspection results
const tabMediaCache = new Map();

// Update extension icon badge
function setDownloadableBadge(tabId, isDownloadable) {
  if (isDownloadable) {
    chrome.action.setBadgeText({ tabId, text: "●" });
    chrome.action.setBadgeBackgroundColor({ tabId, color: "#06D6A0" });
    chrome.action.setTitle({ tabId, title: "yt-mini: Media detected & ready to download!" });
  } else {
    chrome.action.setBadgeText({ tabId, text: "" });
    chrome.action.setTitle({ tabId, title: "yt-mini Media Downloader" });
  }
}

// Quietly check if current tab URL is downloadable via yt-dlp simulation (zero media bandwidth)
async function inspectTabUrl(tabId, url) {
  if (!url || url.startsWith("chrome://") || url.startsWith("brave://") || url.startsWith("edge://") || url.startsWith("about:")) {
    setDownloadableBadge(tabId, false);
    return;
  }

  try {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 4000);

    const resp = await fetch(`${BRIDGE_URL}/inspect`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ url }),
      signal: controller.signal
    });
    clearTimeout(timeoutId);

    if (resp.ok) {
      const data = await resp.json();
      if (data && data.downloadable) {
        tabMediaCache.set(tabId, data);
        setDownloadableBadge(tabId, true);
        return;
      }
    }
  } catch (err) {
    // Companion app is offline or inspection timed out
  }

  tabMediaCache.delete(tabId);
  setDownloadableBadge(tabId, false);
}

// Listen for tab activation (user switches tab)
chrome.tabs.onActivated.addListener(async (activeInfo) => {
  try {
    const tab = await chrome.tabs.get(activeInfo.tabId);
    if (tab && tab.url) {
      inspectTabUrl(tab.id, tab.url);
    }
  } catch (e) {}
});

// Listen for tab URL navigation / updates
chrome.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
  if (changeInfo.status === "complete" && tab.url) {
    inspectTabUrl(tabId, tab.url);
  }
});

// Message listener for popup requests
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  if (request.action === "getCachedMedia") {
    const data = tabMediaCache.get(request.tabId);
    sendResponse({ media: data || null });
  }
  return true;
});
