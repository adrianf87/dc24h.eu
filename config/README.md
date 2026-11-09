# Configuration

Copy `dc24h.ini.example` to a local INI, edit settings, then run:

```sh
./build/dc24h --config config/dc24h.ini --check-config
./build/dc24h --config config/dc24h.ini
```

Nothing is loaded implicitly. CLI overrides file settings; INI paths resolve against the file directory. Keep keys outside Git. See [English (US)](../doc/us/CONFIGURATION.md) | [Polski](../doc/pl/CONFIGURATION.md).

Logging: `level=info|warning|error|off`, `connections=on|off` (default off). See [logging](../doc/us/LOGGING.md).
