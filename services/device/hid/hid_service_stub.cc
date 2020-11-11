// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_service_stub.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "services/device/hid/hid_connection.h"

namespace device {

HidServiceStub::HidServiceStub() = default;

HidServiceStub::~HidServiceStub() = default;

void HidServiceStub::Connect(const std::string& device_id,
                             ConnectCallback callback) /*override*/ {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), nullptr));
}

base::WeakPtr<HidService> HidServiceStub::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
