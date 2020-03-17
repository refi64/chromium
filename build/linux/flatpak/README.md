## Building a Chromium Flatpak

### Build arguments to pass to gn

```
use_udev = false
# Not required but makes builds faster.
use_lld = true
# NaCL hasn't been tested and mostly doesn't work yet.
enable_nacl = false
# Unrelated to Flatpak but helps speed up builds.
blink_symbol_level = 0
use_gnome_keyring = false
# use_sysroot = false
```

### Replacing system libraries

You may replace the following libraries with system versions (see ../unbundle/README):

```
flac
libdrm
libjpeg
libpng
libvpx
libwebp
libxml
libxslt
opus
```

Note that you can NOT use these with Goma.
