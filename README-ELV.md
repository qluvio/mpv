

# Build

## Dependencies - Mac OSX

  `brew install lua@5.1`
  `brew install youtube-dl`

## Dependencies - Ubuntu

  ` apt install \
      lua5.1 \
      luajit \
      liblua5.1-dev \
      libluajit-5.1-dev \
      youtube-dl
  `

## Configure and build

### Mac OSX

```
export PKG_CONFIG_PATH=/s/QCODE/elv-toolchain/dist/darwin-10.14/lib/pkgconfig:/usr/local/Cellar/lua\@5.1/5.1.5_8/libexec/lib/pkgconfig
```

### Ubuntu

> Notes:
> - compile elv-toolchain FFMpeg with `--enable-openssl`
> - replace `...` with path to elv-toolchain

```
export PKG_CONFIG_PATH=.../elv-toolchain/dist/linux-glibc.2.27/lib/pkgconfig
```

Install waf if not installed - run `./bootstrap.py`

Prerequisites:
- libass-dev

```
./waf configure --disable-gl --disable-x11

LD_RUN_PATH='$ORIGIN/../lib' RPATH='$ORIGIN/../lib' ./waf     # This builds ./build/mpv
```

# Use it

Ubuntu
```
export LD_LIBRARY_PATH=/opt/eluvio/src/elv-toolchain/dist/linux-glibc.2.27/lib
```

```
./build/mpv http://lcoalhost:8008/...
```



# Research

```

stream_read_unbuffered 8k size 1633

```

