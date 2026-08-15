/**
 * @file arena.cpp
 * @author Jaroslav Hucel (xhucel00@vutbr.cz)
 * @brief Implementation of Arena allocator
 *
 * @date Created: 12. 10. 2025
 * @date Modified: 10. 08. 2026
 *
 * @copyright Copyright (c) 2025 -> Public Domain, for more information see LICENSE
 */

#include "arena.hpp"

#include <cstring>
#include <cstdint>

using namespace vkgen;

void* Arena::allocate(size_t size, size_t alignment) {
    size_t space = remaining;
    void* ptr = current;

    // Round ptr up to the next multiple of alignment. Works because alignment is a power of two:
    // (alignment - 1) has all low bits set; ~(alignment - 1) masks them off.
    std::uintptr_t aligned = (reinterpret_cast<std::uintptr_t>(ptr) + (alignment - 1)) & ~(alignment - 1);
    size_t padding = aligned - reinterpret_cast<std::uintptr_t>(ptr);

    if (padding + size > space) {
        // New block must fit worst-case padding on top of size, so include `alignment` in the request.
        allocateBlock(std::max(DEFAULT_BLOCK_SIZE, size + alignment));
        return allocate(size, alignment); // retry in the fresh block
    }

    current = reinterpret_cast<char*>(aligned + size);
    remaining = space - (padding + size);
    return reinterpret_cast<void*>(aligned);
}


void Arena::reset() noexcept {
    current = nullptr;
    remaining = 0;
    blocks.clear();
}


void Arena::allocateBlock(size_t size) {
    auto block = std::make_unique<char[]>(size);
    current = block.get();
    remaining = size;
    blocks.push_back(std::move(block));
}


std::string_view Arena::storeString(const sv& str) {
    void* mem = allocate(str.size() + 1, alignof(char)); // +1 for null terminator
    char* dst = reinterpret_cast<char*>(mem);
    std::memcpy(dst, str.data(), str.size());
    dst[str.size()] = '\0';
    return { dst, str.size() };
}
