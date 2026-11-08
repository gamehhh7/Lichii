// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <vector>

#include "common/core_pinning.h"

namespace Common {

bool PreferLargestLastLevelCache(std::span<LogicalProcessor> processors) {
    if (processors.empty()) {
        return false;
    }
    const auto [slowest, fastest] =
        std::ranges::minmax(processors, {}, &LogicalProcessor::efficiency_class);
    const auto [smallest, largest] =
        std::ranges::minmax(processors, {}, &LogicalProcessor::last_level_cache_size);
    if (slowest.efficiency_class != fastest.efficiency_class ||
        smallest.last_level_cache_size == 0 ||
        smallest.last_level_cache_size == largest.last_level_cache_size) {
        return false;
    }
    // 7900X3D is 12 cores and 7950X3D is 16; both are two CCDs with unequal L3. The 6- and
    // 8-core X3D parts, including the Ryzen 7 5700X3D, are one CCD. Do not invent a second CCD
    // from a split cache report on those chips.
    std::vector<u32> cores;
    cores.reserve(processors.size());
    for (const LogicalProcessor& processor : processors) {
        cores.push_back(processor.core);
    }
    std::ranges::sort(cores);
    cores.erase(std::unique(cores.begin(), cores.end()), cores.end());
    constexpr std::size_t MinPhysicalCoresForDualCcd = 12;
    if (cores.size() < MinPhysicalCoresForDualCcd) {
        return false;
    }
    for (LogicalProcessor& processor : processors) {
        if (processor.last_level_cache_size == largest.last_level_cache_size) {
            ++processor.efficiency_class;
        }
    }
    return true;
}

GpuThreadPinning PlanGpuThreadPinning(std::span<const LogicalProcessor> processors) {
    GpuThreadPinning plan;
    if (processors.empty()) {
        return plan;
    }
    const u8 fastest =
        std::ranges::max(processors, {}, &LogicalProcessor::efficiency_class).efficiency_class;
    std::vector<u32> cores;
    for (const LogicalProcessor& processor : processors) {
        if (processor.efficiency_class == fastest) {
            cores.push_back(processor.core);
        }
    }
    std::ranges::sort(cores);
    cores.erase(std::unique(cores.begin(), cores.end()), cores.end());
    plan.performance_cores = cores.size();
    if (cores.size() < MinCoresForGpuThreadPinning) {
        return plan;
    }
    const u32 front_core = cores[1];
    const u32 back_core = cores[2];
    for (const LogicalProcessor& processor : processors) {
        if (processor.efficiency_class != fastest) {
            continue;
        }
        if (processor.core == front_core) {
            plan.front.push_back(processor.id);
        } else if (processor.core == back_core) {
            plan.back.push_back(processor.id);
        } else {
            plan.guest.push_back(processor.id);
        }
    }
    return plan;
}

} // namespace Common
