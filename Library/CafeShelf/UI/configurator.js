'use strict';
const result = document.getElementById('result');
const pending = new Map();
let nextId = 0;
function request(op, payload = {}, files) {
  const id = ++nextId;
  return new Promise((resolve, reject) => {
    pending.set(id, {resolve, reject});
    try {
      const message = JSON.stringify({id, op, payload});
      if (files) chrome.webview.postMessageWithAdditionalObjects(message, files);
      else chrome.webview.postMessage(message);
    } catch (error) {
      pending.delete(id);
      reject(new Error('Windows could not receive this selection. Drag it directly from File Explorer.'));
    }
  });
}
chrome.webview.addEventListener('message', event => {
  const response = event.data;
  const waiter = response && pending.get(response.id);
  if (!waiter) return;
  pending.delete(response.id);
  if (response.ok) waiter.resolve(response.data);
  else waiter.reject(new Error(response.data?.message || response.code || 'The operation failed.'));
});
async function select(op, purpose, files) {
  result.textContent = 'Waiting for selection…';
  try {
    const data = await request(op, purpose ? {purpose} : {}, files);
    result.textContent = data.path ? 'Selected: ' + data.path : 'Selection cleared.';
  } catch (error) { result.textContent = error.message; }
}
document.getElementById('browse').addEventListener('click', () => select('browseLauncher'));
document.getElementById('folder').addEventListener('click', () => select('browseFolder'));
document.getElementById('browseIcon').addEventListener('click', () => select('browseIcon'));
document.getElementById('cancel').addEventListener('click', () => select('cancelDraft'));
document.getElementById('lock').addEventListener('click', () => {
  request('lockNow').catch(error => { result.textContent = error.message; });
});
for (const purpose of ['launcher', 'icon']) {
  const target = document.getElementById(purpose);
  target.addEventListener('dragover', event => {
    event.preventDefault();
    event.dataTransfer.dropEffect = 'copy';
    target.classList.add('over');
  });
  target.addEventListener('dragleave', () => target.classList.remove('over'));
  target.addEventListener('drop', event => {
    event.preventDefault();
    target.classList.remove('over');
    const files = Array.from(event.dataTransfer.files);
    if (files.length !== 1) { result.textContent = 'Drop one file or folder at a time.'; return; }
    select('importDrop', purpose, files);
  });
}
window.addEventListener('dragover', event => event.preventDefault());
window.addEventListener('drop', event => event.preventDefault());
request('load').catch(error => { result.textContent = error.message; });
