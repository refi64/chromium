// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_SERVICE_STUB_H_
#define SERVICES_DEVICE_HID_HID_SERVICE_STUB_H_

#include "base/memory/weak_ptr.h"
#include "services/device/hid/hid_service.h"

namespace device {

class HidServiceStub : public HidService {
 public:
  HidServiceStub();
  ~HidServiceStub() override;

  // HidService:
  void Connect(const std::string& device_id, ConnectCallback callback) override;
  base::WeakPtr<HidService> GetWeakPtr() override;

 private:
  base::WeakPtrFactory<HidServiceStub> weak_factory_{this};
};

}  // namespace device

#endif
