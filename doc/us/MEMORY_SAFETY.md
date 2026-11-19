# Memory safety — alpha-0.9.15

AddressSanitizer, UndefinedBehaviorSanitizer and ThreadSanitizer. The branch starts from main `a773395`. The branch name is `alpha-0.9.15`.

Each sanitizer is a separate Debug build. An empty switch leaves the hub, the protocol and the tests on the normal build. There is no new route, INI key, schema migration, Apache setting, CLI flag, WebAdmin page or status field. A passing probe is not a substitute for the suite. This is not a capacity claim.

## Build modes

Configure one mode. AddressSanitizer and ThreadSanitizer cannot share a binary.

```sh
cmake -S . -B build-address -DCMAKE_BUILD_TYPE=Debug -DDC24H_SANITIZER=address -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake -S . -B build-undefined -DCMAKE_BUILD_TYPE=Debug -DDC24H_SANITIZER=undefined -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake -S . -B build-thread -DCMAKE_BUILD_TYPE=Debug -DDC24H_SANITIZER=thread -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
```

`address` compiles and links `-fsanitize=address` and halts on the first error, including leaks. `undefined` compiles and links `-fsanitize=undefined` and halts on the first error. `thread` compiles and links `-fsanitize=thread` and halts on the first data race in hub code. OpenSSL is not instrumented. Its own locks protect simultaneous password derivations, so this mode suppresses reports whose interceptor caller is `libcrypto.so` or `libssl.so`. A race in hub code still fails the run. Any other value fails configuration. The normal Debug and Release builds leave `DC24H_SANITIZER` empty.

CTest on that build sets `ASAN_OPTIONS`, `UBSAN_OPTIONS` or `TSAN_OPTIONS`. It also sets `DC24H_SANITIZER` to the mode name.

## Probe

`memory.safety` allocates a small array, joins two threads on an atomic counter and a mutex counter, and multiplies two in-range signed integers. The AddressSanitizer and ThreadSanitizer builds fail the probe when the translation unit is not instrumented. The success line is `Alpha 0.9.15 memory safety probe OK` followed by `mode=address`, `mode=undefined`, `mode=thread` or `mode=none`.

## Sandbox

The shipped systemd unit still sets `MemoryDenyWriteExecute=yes`. That setting stays. The live sandbox launch skips an AddressSanitizer or ThreadSanitizer hub, because those runtimes cannot map their shadow under that restriction. The unit text, the private sysusers check and the host-write rejection still run. UndefinedBehaviorSanitizer still launches the hub in the sandbox.

## Limits

GitHub Check status still builds Debug and Release on Ubuntu 24.04. It does not build the three sanitizer trees. Schema v9 is unchanged. `configuration.manage` and `hub.manage` stay unimplemented. Browser kick and mute stay undispatched. Status and health stay bearer-only. `Require local` stays. No residual threat is closed.
