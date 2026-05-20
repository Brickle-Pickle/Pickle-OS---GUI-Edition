# Vendored Lua 5.4

This folder hosts the Lua 5.4 source compiled as a PlatformIO library so the device can run downloaded Lua game scripts.

## One-time setup

Download the official Lua tarball and drop its `src/` folder here:

```bash
cd lib/lua
curl -L -o lua-5.4.7.tar.gz https://www.lua.org/ftp/lua-5.4.7.tar.gz
tar -xzf lua-5.4.7.tar.gz
mv lua-5.4.7/src ./src
rm -rf lua-5.4.7 lua-5.4.7.tar.gz
```

After this the layout must be:

```text
lib/lua/
    library.json
    README.md
    src/
        lapi.c lapi.h
        lauxlib.c lauxlib.h
        ... (the rest of the official Lua 5.4 source)
```

The build is configured in `library.json`. It compiles every `.c` except the standalone CLIs (`lua.c`, `luac.c`, `onelua.c`, `ltests.c`) and uses the C89 numeric path so it runs cleanly on the ESP32 toolchain.
