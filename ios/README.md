# AetherVM for iOS
AetherVM for non-jailbroken iOS.

## Build
To build your own version, you need to firstly build **AetherVM** for M Silicon macOS, and have **brotli** command line in current terminal session, which will be used to decompress the `arm64.opc.br` from [AetherVMExt](https://github.com/AetherVM/AetherVMExt). 

If everything's ready, then build it in one go:
```sh
icpp build.cc
```
