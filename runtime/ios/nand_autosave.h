#pragma once
// Deferred save-tree backups after NAND writes (stage clear / menu save).
// Request from the IOS flush path; Tick from the present/UI thread.

void NandAutosave_OnFlush();   // guest/IOS thread — sets a pending flag when enabled
void NandAutosave_Tick();      // host present thread — debounce, backup, prune
