#pragma once
#include <cstddef>
#include <filesystem>

// Write `data` to `dest` via a sibling `.tmp` then rename.
// On any failure: leave an existing `dest` untouched, remove the temp file, return false.
bool Fs_WriteFileAtomic(const std::filesystem::path& dest, const void* data, size_t size);
