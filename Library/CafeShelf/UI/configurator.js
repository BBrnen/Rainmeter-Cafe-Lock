'use strict';
// ShelfSuite UI adaptation, copyright (c) 2025 Martin Santos, MIT.
// Disk writes and selections are owned exclusively by the native Maintenance session.
const $ = id => document.getElementById(id);
let nextRequest = 0;
const pending = new Map();
function request(op, payload = {}, files = []) {
    return new Promise((resolve, reject) => {
        const id = ++nextRequest;
        const timer = setTimeout(() => {
            pending.delete(id);
            reject(new Error('The operation is taking too long. Reload the shelf before trying another save.'));
        }, 60000);
        pending.set(id, { resolve, reject, timer });
        try {
            const message = JSON.stringify({ id, op, payload });
            if (files.length) chrome.webview.postMessageWithAdditionalObjects(message, files);
            else chrome.webview.postMessage(message);
        } catch (error) {
            clearTimeout(timer); pending.delete(id); reject(error);
        }
    });
}
chrome.webview.addEventListener('message', event => {
    const result = event.data;
    const entry = result && pending.get(result.id);
    if (!entry) return;
    clearTimeout(entry.timer); pending.delete(result.id);
    if (result.ok) entry.resolve(result.data);
    else { const error = new Error(result.data?.message || 'The operation could not complete.'); error.code = result.code; entry.reject(error); }
});
const icons = ['folder.png','calculator.png','calendar.png','chat.png','cloud.png','code.png','download.png','file.png','game.png','link.png','mail.png','music.png','news.png','office.png','photo.png','settings.png','storage.png','terminal.png','video.png','web.png'];
for (const value of icons) { const option = document.createElement('option'); option.value = value; $('stockIcons').append(option); }
let shelves = [], shelfId = '', tabIndex = 0, draft = null, draftGeneration = 0, working = false, tabKind = '', confirmAction = null;
const currentShelf = () => shelves.find(shelf => shelf.id === shelfId);
function node(tag, text, className) {
    const element = document.createElement(tag);
    if (text !== undefined) element.textContent = text;
    if (className) element.className = className;
    return element;
}
function button(text, action, className) {
    const element = node('button', text, className); element.type = 'button';
    element.addEventListener('click', action); return element;
}
function status(text) { $('status').textContent = text; }
function render() {
    $('shelvesNav').replaceChildren(); $('tabsNav').replaceChildren(); $('itemsList').replaceChildren();
    for (const shelf of shelves) {
        const entry = button(shelf.id, () => { shelfId = shelf.id; tabIndex = 0; render(); }, 'sidebar-item');
        entry.setAttribute('aria-current', String(shelf.id === shelfId)); entry.disabled = working;
        $('shelvesNav').append(entry);
    }
    const shelf = currentShelf(), tabs = shelf?.tabs || [];
    if (tabIndex >= tabs.length) tabIndex = 0;
    $('shelfTitle').textContent = shelf?.id || 'No shelves available';
    $('shelfError').textContent = shelf?.error || '';
    tabs.forEach((tab, index) => {
        const entry = button(tab.name, () => { tabIndex = index; render(); });
        entry.setAttribute('aria-current', String(index === tabIndex)); entry.disabled = working;
        $('tabsNav').append(entry);
    });
    const items = tabs[tabIndex]?.items || [];
    items.forEach((item, index) => {
        const row = node('div', undefined, 'launcher-row'), copy = node('div', undefined, 'launcher-copy');
        copy.append(node('div', item.label, 'item-label'), node('div', item.action, 'item-action'));
        const edit = button('Edit', () => openItem(index), 'edit-item');
        const remove = button('Remove', () => confirm('Remove "' + item.label + '" from this shelf?', () => saveEdit('removeItem', index)), 'remove-item');
        edit.disabled = remove.disabled = working; row.append(copy, edit, remove); $('itemsList').append(row);
    });
    $('capacity').textContent = shelf && !shelf.error ? items.length + ' / ' + shelf.itemCapacity + ' launchers in this tab' : '';
    $('addItemBtn').disabled = working || !tabs.length || items.length >= (shelf?.itemCapacity || 0) || !!shelf?.error;
    $('addTabBtn').disabled = working || !shelf || !!shelf.error || tabs.length >= (shelf?.tabCapacity || 0);
    $('renameTabBtn').disabled = working || !tabs.length;
    $('removeTabBtn').disabled = working || tabs.length <= 1;
    $('reloadBtn').disabled = working;
}
async function reload(message = '') {
    const result = await request('load');
    shelves = result.shelves || [];
    if (!shelves.some(shelf => shelf.id === shelfId)) shelfId = shelves[0]?.id || '';
    render(); if (message) status(message); else status(result.message || 'Choose a shelf and tab, then Add Item or Edit.');
}
function setWorking(value) {
    working = value;
    for (const id of ['browseLauncher','browseFolder','browseIcon','itemSaveBtn','itemCancelBtn','itemLabel','itemAction','itemIcon','tabSaveBtn','tabCancelBtn']) $(id).disabled = value;
    render();
}
function openItem(index = null) {
    if (working) return;
    ++draftGeneration;
    const item = index === null ? {label:'', action:'', icon:currentShelf()?.defaultIcon || 'file.png'} : currentShelf().tabs[tabIndex].items[index];
    draft = {index, iconId:0};
    $('itemModalHeader').textContent = index === null ? 'Add launcher' : 'Edit launcher';
    $('itemLabel').value = item.label; $('itemAction').value = item.action; $('itemIcon').value = item.icon;
    $('itemError').textContent = ''; $('iconStatus').textContent = 'Keep an existing icon, or import a custom one.';
    $('iconPreview').hidden = true; $('iconPreview').removeAttribute('src');
    $('itemModal').showModal(); $('itemLabel').focus();
}
async function cancelItem() {
    if (working) return;
    ++draftGeneration; draft = null; $('itemModal').close();
    try { await request('cancelDraft'); } catch (error) { status(error.message); }
}
async function importItem(op, purpose, files = []) {
    if (working || !draft) return;
    const generation = draftGeneration;
    setWorking(true); $('itemError').textContent = '';
    try {
        const result = await request(op, op === 'importDrop' ? {purpose} : {}, files);
        if (!draft || generation !== draftGeneration) return;
        if (purpose === 'launcher') { $('itemLabel').value = result.name; $('itemAction').value = result.action; }
        if (result.iconId) {
            draft.iconId = result.iconId;
            $('itemIcon').value = result.iconName;
            $('iconPreview').src = result.preview; $('iconPreview').hidden = false;
            $('iconStatus').textContent = 'Ready to import. A unique PNG filename will be chosen when you save.';
        } else if (result.warning) {
            draft.iconId = 0; $('iconPreview').hidden = true;
            $('iconStatus').textContent = result.warning + ' The existing/default icon will be kept.';
        }
    } catch (error) {
        if (error.code !== 'Cancelled') $('itemError').textContent = error.message;
    } finally { setWorking(false); }
}
function editPayload(kind, item = 0, label = '', action = '', icon = '') {
    return {kind, tab:tabIndex, item, label, action, icon};
}
async function saveEdit(kind, item = 0, label = '', action = '', icon = '', iconId = 0) {
    if (working) return;
    const shelf = currentShelf(); if (!shelf || shelf.error) return;
    setWorking(true);
    try {
        const result = await request('saveEdits', {shelf:shelf.id, version:shelf.version, iconId, edit:editPayload(kind,item,label,action,icon)});
        ++draftGeneration; draft = null; $('itemModal').close(); $('tabModal').close(); $('confirmModal').close();
        await reload('Saved.' + (result.backup ? ' A recovery copy was kept.' : ''));
    } catch (error) {
        if ($('itemModal').open) $('itemError').textContent = error.message;
        else if ($('tabModal').open) $('tabError').textContent = error.message;
        else status(error.message);
    } finally { setWorking(false); }
}
function confirm(text, action) { if(working)return; confirmAction = action; $('confirmText').textContent = text; $('confirmModal').showModal(); }
function openTab(kind) {
    if(working)return; tabKind = kind;
    $('tabHeading').textContent = kind === 'addTab' ? 'Add tab' : 'Rename tab';
    $('tabName').value = kind === 'addTab' ? '' : currentShelf().tabs[tabIndex].name;
    $('tabError').textContent = ''; $('tabModal').showModal(); $('tabName').focus();
}
$('addItemBtn').addEventListener('click', () => openItem());
$('browseLauncher').addEventListener('click', () => importItem('browseLauncher', 'launcher'));
$('browseFolder').addEventListener('click', () => importItem('browseFolder', 'launcher'));
$('browseIcon').addEventListener('click', () => importItem('browseIcon', 'icon'));
$('itemIcon').addEventListener('input', () => { if (draft) draft.iconId = 0; $('iconPreview').hidden = true; $('iconStatus').textContent = 'Using the named existing icon.'; });
$('itemCancelBtn').addEventListener('click', cancelItem);
$('itemModal').addEventListener('cancel', event => { event.preventDefault(); cancelItem(); });
$('itemForm').addEventListener('submit', event => {
    event.preventDefault(); if (!draft || working) return;
    saveEdit(draft.index === null ? 'addItem' : 'setItem', draft.index || 0, $('itemLabel').value, $('itemAction').value, $('itemIcon').value, draft.iconId);
});
$('addTabBtn').addEventListener('click', () => openTab('addTab'));
$('renameTabBtn').addEventListener('click', () => openTab('renameTab'));
$('removeTabBtn').addEventListener('click', () => confirm('Remove this tab and its launcher entries? The old config will be kept as a recovery copy.', () => saveEdit('removeTab')));
$('tabForm').addEventListener('submit', event => { event.preventDefault(); saveEdit(tabKind,0,$('tabName').value); });
$('tabCancelBtn').addEventListener('click', () => { if(!working)$('tabModal').close(); });
$('tabModal').addEventListener('cancel', event => { if(working)event.preventDefault(); });
$('confirmCancel').addEventListener('click', () => { if(!working)$('confirmModal').close(); });
$('confirmRemove').addEventListener('click', () => { if(!working && confirmAction)confirmAction(); });
$('confirmModal').addEventListener('cancel', event => { if(working)event.preventDefault(); });
$('reloadBtn').addEventListener('click', async () => {
    if(working)return; setWorking(true);
    try { await reload(); } catch(error) {status(error.message);} finally {setWorking(false);}
});
$('lockNow').addEventListener('click', () => { ++draftGeneration; request('lockNow').catch(() => {}); });
for (const [id, purpose] of [['launcherDrop','launcher'],['iconDrop','icon']]) {
    const zone = $(id);
    zone.addEventListener('dragover', event => {event.preventDefault(); if(!working)zone.classList.add('over'); event.dataTransfer.dropEffect = working ? 'none' : 'copy';});
    zone.addEventListener('dragleave', () => zone.classList.remove('over'));
    zone.addEventListener('drop', event => {
        event.preventDefault(); zone.classList.remove('over');
        if(working)return;
        const files = [...event.dataTransfer.files];
        if(files.length !== 1) { $('itemError').textContent = 'Drop one file or folder at a time.'; return; }
        importItem('importDrop', purpose, files);
    });
}
document.addEventListener('dragover', event => event.preventDefault());
document.addEventListener('drop', event => event.preventDefault());
setWorking(true);
reload().catch(error => status(error.message)).finally(() => setWorking(false));
