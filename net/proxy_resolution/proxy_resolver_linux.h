// Copyright (c) 2020 Endless OS Foundation LLC
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_RESOLVER_LINUX_H_
#define NET_PROXY_RESOLUTION_PROXY_RESOLVER_LINUX_H_

#include "base/macros.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/proxy_resolver_factory.h"
#include "url/gurl.h"

namespace net {

class NET_EXPORT ProxyResolverFactoryLinux : public ProxyResolverFactory {
 public:
  ProxyResolverFactoryLinux();

  int CreateProxyResolver(const scoped_refptr<PacFileData>& pac_script,
                          std::unique_ptr<ProxyResolver>* resolver,
                          CompletionOnceCallback callback,
                          std::unique_ptr<Request>* request) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyResolverFactoryLinux);
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_RESOLVER_LINUX_H_
