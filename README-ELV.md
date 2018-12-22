

# Build

## Dependencies - Mac OSX

  `brew install lua@5.1`
  `brew install youtube-dl`

## Configure and build

```
export PKG_CONFIG_PATH=/s/QCODE/elv-toolchain/dist/darwin-10.14/lib/pkgconfig:/usr/local/Cellar/lua\@5.1/5.1.5_8/libexec/lib/pkgconfig
```

Install waf - run ./bootstrap.py

```
./waf configure --disable-libass

./waf     # This builds ./build/mpv
```


# Use it

```
./build/mpv http://lcoalhost:8008/...
```



# Research

```

stream_read_unbuffered 8k size 1633

```
