// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/scoped_file.h"
#include "base/no_destructor.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/platform_util_internal.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_features.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "ui/gtk/select_file_dialog_impl_portal.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace platform_util {

namespace {

const char kFreedesktopFileManagerName[] = "org.freedesktop.FileManager1";
const char kFreedesktopFileManagerPath[] = "/org/freedesktop/FileManager1";

const char kMethodShowItems[] = "ShowItems";

const char kXdgPortalService[] = "org.freedesktop.portal.Desktop";
const char kXdgPortalPath[] = "/org/freedesktop/portal/desktop";
const char kOpenURIInterface[] = "org.freedesktop.portal.OpenURI";

const char kMethodOpenDirectory[] = "OpenDirectory";

class ShowItemHelper : public content::NotificationObserver {
 public:
  static ShowItemHelper& GetInstance() {
    static base::NoDestructor<ShowItemHelper> instance;
    return *instance;
  }

  ShowItemHelper() {
    registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                   content::NotificationService::AllSources());
  }

  ShowItemHelper(const ShowItemHelper&) = delete;
  ShowItemHelper& operator=(const ShowItemHelper&) = delete;

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
    // The browser process is about to exit. Clean up while we still can.
    if (bus_)
      bus_->ShutdownOnDBusThreadAndBlock();
    bus_.reset();
    object_proxy_ = nullptr;
  }

  void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
    if (!bus_) {
      // Sets up the D-Bus connection.
      dbus::Bus::Options bus_options;
      bus_options.bus_type = dbus::Bus::SESSION;
      bus_options.connection_type = dbus::Bus::PRIVATE;
      bus_options.dbus_task_runner = dbus_thread_linux::GetTaskRunner();
      bus_ = base::MakeRefCounted<dbus::Bus>(bus_options);
    }

    if (base::FeatureList::IsEnabled(features::kXdgFileChooserPortal)) {
      ShowItemInFolderUsingXdgPortal(profile, full_path);
    } else {
      ShowItemInFolderUsingFileManager(profile, full_path);
    }
  }

  void ShowItemInFolderUsingXdgPortal(Profile* profile,
                                      const base::FilePath& full_path) {
    if (!object_proxy_) {
      object_proxy_ = bus_->GetObjectProxy(kXdgPortalService,
                                           dbus::ObjectPath(kXdgPortalPath));
    }

    base::ScopedFD fd(open(full_path.value().c_str(), O_RDONLY | O_CLOEXEC));
    if (!fd.is_valid()) {
      PLOG(ERROR) << "Failed to open " << full_path.value();
      // At least open the parent folder if possible.
      OpenItem(profile, full_path.DirName(), OPEN_FOLDER,
               OpenOperationCallback());
      return;
    }

    dbus::MethodCall open_directory_call(kOpenURIInterface,
                                         kMethodOpenDirectory);
    dbus::MessageWriter writer(&open_directory_call);

    writer.AppendString("");  // parent window
    writer.AppendFileDescriptor(fd.get());

    dbus::MessageWriter options_writer(nullptr);
    writer.OpenArray("{sv}", &options_writer);
    writer.CloseContainer(&options_writer);

    PerformBusCall(&open_directory_call, profile, full_path);
  }

 private:
  void ShowItemInFolderUsingFileManager(Profile* profile,
                                        const base::FilePath& full_path) {
    if (!object_proxy_) {
      object_proxy_ =
          bus_->GetObjectProxy(kFreedesktopFileManagerName,
                               dbus::ObjectPath(kFreedesktopFileManagerPath));
    }

    dbus::MethodCall show_items_call(kFreedesktopFileManagerName,
                                     kMethodShowItems);
    dbus::MessageWriter writer(&show_items_call);

    writer.AppendArrayOfStrings(
        {"file://" + full_path.value()});  // List of file(s) to highlight.
    writer.AppendString({});               // startup-id

    PerformBusCall(&show_items_call, profile, full_path);
  }

  void PerformBusCall(dbus::MethodCall* call,
                      Profile* profile,
                      const base::FilePath& full_path) {
    // Skip opening the folder during browser tests, to avoid leaving an open
    // file explorer window behind.
    if (!internal::AreShellOperationsAllowed())
      return;

    object_proxy_->CallMethod(
        call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ShowItemHelper::ShowItemInFolderResponse,
                       weak_ptr_factory_.GetWeakPtr(), profile,
                       call->GetMember(), full_path));
  }

  void ShowItemInFolderResponse(Profile* profile,
                                const std::string& method,
                                const base::FilePath& full_path,
                                dbus::Response* response) {
    if (response)
      return;

    LOG(ERROR) << "Error calling " << method;
    // If the method call fails, at least open the parent folder.
    OpenItem(profile, full_path.DirName(), OPEN_FOLDER,
             OpenOperationCallback());
  }

  content::NotificationRegistrar registrar_;

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* object_proxy_ = nullptr;

  base::WeakPtrFactory<ShowItemHelper> weak_ptr_factory_{this};
};

void RunCommand(const std::string& command,
                const base::FilePath& working_directory,
                const std::string& arg) {
  std::vector<std::string> argv;
  argv.push_back(command);
  argv.push_back(arg);

  base::LaunchOptions options;
  options.current_directory = working_directory;
  options.allow_new_privs = true;
  // xdg-open can fall back on mailcap which eventually might plumb through
  // to a command that needs a terminal.  Set the environment variable telling
  // it that we definitely don't have a terminal available and that it should
  // bring up a new terminal if necessary.  See "man mailcap".
  options.environment["MM_NOTTTY"] = "1";

  // In Google Chrome, we do not let GNOME's bug-buddy intercept our crashes.
  // However, we do not want this environment variable to propagate to external
  // applications. See http://crbug.com/24120
  char* disable_gnome_bug_buddy = getenv("GNOME_DISABLE_CRASH_DIALOG");
  if (disable_gnome_bug_buddy &&
      disable_gnome_bug_buddy == std::string("SET_BY_GOOGLE_CHROME"))
    options.environment["GNOME_DISABLE_CRASH_DIALOG"] = std::string();

  base::Process process = base::LaunchProcess(argv, options);
  if (process.IsValid())
    base::EnsureProcessGetsReaped(std::move(process));
}

void XDGOpen(const base::FilePath& working_directory, const std::string& path) {
  RunCommand("xdg-open", working_directory, path);
}

void XDGEmail(const std::string& email) {
  RunCommand("xdg-email", base::FilePath(), email);
}

}  // namespace

namespace internal {

void PlatformOpenVerifiedItem(const base::FilePath& path, OpenItemType type) {
  // May result in an interactive dialog.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  switch (type) {
    case OPEN_FILE:
      XDGOpen(path.DirName(), path.value());
      break;
    case OPEN_FOLDER:
      // The utility process checks the working directory prior to the
      // invocation of xdg-open by changing the current directory into it. This
      // operation only succeeds if |path| is a directory. Opening "." from
      // there ensures that the target of the operation is a directory.  Note
      // that there remains a TOCTOU race where the directory could be unlinked
      // between the time the utility process changes into the directory and the
      // time the application invoked by xdg-open inspects the path by name.
      XDGOpen(path, ".");
      break;
  }
}

}  // namespace internal

void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ShowItemHelper::GetInstance().ShowItemInFolder(profile, full_path);
}

void OpenExternal(Profile* profile, const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (url.SchemeIs("mailto"))
    XDGEmail(url.spec());
  else
    XDGOpen(base::FilePath(), url.spec());
}

}  // namespace platform_util
