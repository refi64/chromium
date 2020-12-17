// Copyright (c) 2020 Endless OS Foundation LLC
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_resolver_linux.h"

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/net_errors.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "sandbox/linux/services/flatpak_sandbox.h"

#if defined(USE_GIO)
#include <gio/gio.h>
#endif  // defined(USE_GIO)

namespace net {

namespace {

#if defined(USE_GIO)
// Same TTL used for DNS caches, see net/dns/host_resolver_manager.cc
static const unsigned kCacheExpireSeconds = 60;

class ProxyResolverLinuxImplPortal : public ProxyResolver {
 public:
  explicit ProxyResolverLinuxImplPortal();
  ~ProxyResolverLinuxImplPortal() override;

  // ProxyResolver methods:
  int GetProxyForURL(const GURL& url,
                     const NetworkIsolationKey& network_isolation_key,
                     ProxyInfo* results,
                     CompletionOnceCallback callback,
                     std::unique_ptr<Request>* request,
                     const NetLogWithSource& net_log) override;

 private:
  void InvalidateCache();

  // Both InvalidateCache and GetProxyForURL run on the same thread,
  // no need for explicit synchronization here
  std::map<std::string, std::string> cache_;
  base::RepeatingTimer cacheInvalidateTimer_;

  GCancellable* cancellable_;
};

ProxyResolverLinuxImplPortal::ProxyResolverLinuxImplPortal()
{
  cancellable_ = g_cancellable_new();

  cacheInvalidateTimer_.Start(FROM_HERE,
      base::TimeDelta::FromSeconds(kCacheExpireSeconds),
      this, &ProxyResolverLinuxImplPortal::InvalidateCache);
}

ProxyResolverLinuxImplPortal::~ProxyResolverLinuxImplPortal()
{
  g_cancellable_cancel(cancellable_);
  g_clear_object(&cancellable_);
  cacheInvalidateTimer_.Stop();
}

// Runs on the worker thread
int ProxyResolverLinuxImplPortal::GetProxyForURL(
    const GURL& query_url,
    const NetworkIsolationKey& /* network_isolation_key */,
    ProxyInfo* results,
    CompletionOnceCallback /* callback */,
    std::unique_ptr<Request>* /* request */,
    const NetLogWithSource& /* net_log */) {
  std::map<std::string, std::string>::const_iterator it =
      cache_.find(query_url.spec());
  if (it != cache_.end()) {
    results->UseNamedProxy(it->second);
    return OK;
  }

  GProxyResolver *resolver = g_proxy_resolver_get_default();
  // TODO: support async resolution if 'request' is non-null - the current
  // consumer is MultiThreadProxyResolver which always expects a sync
  // resolver
  // TODO: handle/warn on error
  g_auto(GStrv) proxy_list = g_proxy_resolver_lookup(resolver,
      query_url.spec().c_str(), cancellable_, NULL);
  if (proxy_list) {
    g_autofree gchar *proxy_list_str = g_strjoinv(";", proxy_list);
    if (proxy_list_str && proxy_list_str[0] != '\0') {
      cache_[query_url.spec()] = proxy_list_str;
      results->UseNamedProxy(proxy_list_str);
    }
  }

  return OK;
}

void ProxyResolverLinuxImplPortal::InvalidateCache()
{
  cache_.clear();
}

#endif // defined(USE_GIO)

} // namespace

ProxyResolverFactoryLinux::ProxyResolverFactoryLinux()
    : ProxyResolverFactory(false /* expects_pac_bytes */) {
}

int ProxyResolverFactoryLinux::CreateProxyResolver(
    const scoped_refptr<PacFileData>& pac_script,
    std::unique_ptr<ProxyResolver>* resolver,
    CompletionOnceCallback callback,
    std::unique_ptr<Request>* request) {
#if defined(USE_GIO)
  if (sandbox::FlatpakSandbox::GetInstance()->GetSandboxLevel() >
      sandbox::FlatpakSandbox::SandboxLevel::kNone) {
    resolver->reset(new ProxyResolverLinuxImplPortal());
    return OK;
  }
#endif
  return ERR_NOT_IMPLEMENTED;
}

}  // namespace net
