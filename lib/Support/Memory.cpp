//===- Memory.cpp----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Support/Memory.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace eld;

BumpPtrAllocator eld::BAlloc;
StringSaver eld::Saver{BAlloc};
std::vector<SpecificAllocBase *> eld::SpecificAllocBase::Instances;

namespace {
struct AllocStats {
  uint64_t Count = 0;
  uint64_t Bytes = 0;
};

struct AllocState {
  bool Enabled = [] {
    const char *Env = std::getenv("ELD_ALLOC_STATS");
    return Env && *Env;
  }();
  StringMap<AllocStats> Stats;

  void dump() {
    if (!Enabled)
      return;
    auto Emit = [&](raw_ostream &OS) {
      OS << "ELD allocation stats (counts * sizeof(T))\n";
      for (const auto &E : Stats) {
        const AllocStats &S = E.getValue();
        OS << " " << E.getKey() << ": count=" << S.Count << " bytes=" << S.Bytes
           << "\n";
      }
    };

    Emit(errs());
  }
};

AllocState &getAllocState() {
  static std::unique_ptr<AllocState> State = std::make_unique<AllocState>();
  return *State;
}
} // namespace

void eld::noteAlloc(const char *TypeName, size_t Size) {
  AllocState &S = getAllocState();
  if (!S.Enabled)
    return;
  auto &Entry = S.Stats[TypeName];
  Entry.Count += 1;
  Entry.Bytes += Size;
}

void eld::freeArena() {
  if (getAllocState().Enabled)
    getAllocState().dump();
  for (SpecificAllocBase *Alloc : llvm::reverse(SpecificAllocBase::Instances))
    Alloc->reset();
  BAlloc.Reset();
}

const char *eld::getUninitBuffer(uint32_t Sz) {
  return BAlloc.Allocate<char>(Sz);
}
