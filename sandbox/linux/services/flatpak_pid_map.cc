// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/flatpak_pid_map.h"
#include "base/optional.h"

namespace sandbox {

bool FlatpakPidMap::Insert(PidPair pair) {
  if (external_to_relative_.contains(pair.external) ||
      relative_to_external_.contains(pair.relative)) {
    return false;
  }

  external_to_relative_[pair.external] = pair.relative;
  relative_to_external_[pair.relative] = pair.external;
  return true;
}

base::Optional<pid_t> FlatpakPidMap::FindRelativeByExternal(pid_t external) {
  return FindImpl(&external_to_relative_, external);
}

base::Optional<pid_t> FlatpakPidMap::FindExternalByRelative(pid_t relative) {
  return FindImpl(&relative_to_external_, relative);
}

base::Optional<pid_t> FlatpakPidMap::DeleteByExternal(pid_t external) {
  return DeleteImpl(&external_to_relative_, &relative_to_external_, external);
}

base::Optional<pid_t> FlatpakPidMap::DeleteByRelative(pid_t relative) {
  return DeleteImpl(&relative_to_external_, &external_to_relative_, relative);
}

base::Optional<pid_t> FlatpakPidMap::FindImpl(base::flat_map<pid_t, pid_t>* map,
                                              pid_t key) {
  auto it = map->find(key);
  return it != map->end() ? it->second : base::Optional<pid_t>();
}

base::Optional<pid_t> FlatpakPidMap::DeleteImpl(
    base::flat_map<pid_t, pid_t>* map,
    base::flat_map<pid_t, pid_t>* reversed,
    pid_t key) {
  auto it = map->find(key);
  if (it == map->end()) {
    return base::Optional<pid_t>();
  }

  pid_t value = it->second;
  reversed->erase(value);
  map->erase(it);
  return value;
}

}  // namespace sandbox
