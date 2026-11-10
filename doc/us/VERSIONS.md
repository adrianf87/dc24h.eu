# Versions

**Current increment — alpha-0.3.7:** [User Registry](USER_REGISTRY.md) owns bounded SID/CID indexes and a reverse session-owner index privately within SessionManager. Admission is transactional across this registry, NickManager and the Session model. Const User lookups resolve both keys to the same current model; reservations remain until session removal. Earlier module descriptions retain their historical scope.

**Increment delivered in alpha-0.3.5:** [User Core](USER_CORE.md) adds checked UTF-8/NFC Nickname values, shared ADC/User text validation, immutable rename candidates and explicit projection from validated ClientInfo/VerifiedIdentity. Users now links the neutral dc24h_text library using the existing utf8proc package. Manager ownership/uniqueness remains unchanged. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.4:** [Session Manager](SESSION_MANAGER.md) owns SID/CID/nickname indexes together with live models. Const lookups and managed lifecycle/User mutations preserve uniqueness through admission, rename and cleanup. LoginHub retains protocol and output ownership. Earlier descriptions retain their historical delivery scope.

**Increment delivered in alpha-0.3.3:** [User Core](USER_CORE.md) adds an owning public guest identity/profile to Session, with validated full-INF projection and lifecycle-guarded updates. It was developed directly from main with Session Core; main now combines it with the previously merged alpha-0.3.2 Session Manager. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.2:** [Session Manager](SESSION_MANAGER.md) centrally owns all registered lifecycle models, with bounded capacity, stable lookup, phase counts and ordered snapshot pages. Protocol peers borrow models; their queues and public routing indexes remain in ADC. Earlier descriptions retain their historical scope.

| Version | Branch | Scope |
| --- | --- | --- |
| alpha-0.1.1 | alpha-0.1.1 | Core |
| alpha-0.1.2 | alpha-0.1.2 | Network |
| alpha-0.1.3 | alpha-0.1.3 | ADC parser |
| alpha-0.1.4 | alpha-0.1.4 | ADCS/TLS; reviewed chatgpt-alpha-0.1.4 history retained |
| alpha-0.1.5 | alpha-0.1.5 | Configuration: INI, CLI, paths and file logging |
| alpha-0.1.6 | alpha-0.1.6 | Logging: startup, connections, warnings and errors |
| alpha-0.1.7 | alpha-0.1.7 | Bounded IP admission, frame quotas and absolute deadlines |
| alpha-0.1.8 | alpha-0.1.8 | MariaDB connection and migration metadata baseline |
| alpha-0.1.9 | alpha-0.1.9 | Local read-only administrative HTTP/JSON API |
| alpha-0.1.10 | alpha-0.1.10 | React/TypeScript/Vite WebAdmin layout and local read-only adapter |
| alpha-0.1.11 | alpha-0.1.11 | Apache2 static HTTPS template and isolated TLS/access tests |
| alpha-0.2.1 | alpha-0.2.1 | Project: alpha-0.1 audit and alpha-0.2 preparation |
| alpha-0.2.2 | alpha-0.2.2 | ADC Parser: BASE arguments, named fields and bounded scalar syntax |
| alpha-0.2.3 | alpha-0.2.3 | ADC State: independent hub connection state machine |
| alpha-0.2.4 | alpha-0.2.4 | SUP: feature/hash negotiation and state integration |
| alpha-0.2.5 | alpha-0.2.5 | SID: generation, RAII reservations and session binding |
| alpha-0.2.6 | alpha-0.2.6 | INF: client information and Tiger identity validation |
| alpha-0.2.7 | alpha-0.2.7 | CID/PID: dedicated Tiger verification and failure handling |
| alpha-0.2.8 | alpha-0.2.8 | Login: guest NORMAL, bounded TCP/TLS output, public presence |
| alpha-0.2.9 | alpha-0.2.9 | STA/QUI: protocol diagnostics, routing and bounded close |
| alpha-0.2.10 | alpha-0.2.10 | Security: pending sessions, cumulative login and identity-work limits |
| alpha-0.3.1 | alpha-0.3.1 | Session Core: Users lifecycle, stable local IDs and snapshots |
| alpha-0.3.2 | alpha-0.3.2 | Session Manager |
| alpha-0.3.3 | alpha-0.3.3 | User Core |
| alpha-0.3.4 | alpha-0.3.4 | Session Manager: central identity registry |
| alpha-0.3.5 | alpha-0.3.5 | User Core: checked public values and ADC projection |
| alpha-0.3.6 | alpha-0.3.6 | Nick Manager: checked names, bounded ownership and atomic rename |
| alpha-0.3.7 | alpha-0.3.7 | User Registry: SID/CID indexes and session-owner cleanup |

Identify an individual build with its version and Git commit SHA; compiling does not modify tracked source. Each significant change updates CHANGELOG.md. Alpha denotes development status, not complete hub functionality. Tags and production deployment are not created automatically.

Historical stage branches and commit history are preserved. alpha-0.1.4 retains the reference TLS branch through a merge and includes follow-up fixes.

## Historical version — alpha-0.2.7

CMake PROJECT_VERSION and npm package/lock root versions are **0.2.7**; runtime/WebAdmin identify **alpha-0.2.7**. Metadata checks enforce consistency. The branch starts from main at `9d3443a` after the INF merge and adds [dedicated CID/PID verification](ADC_IDENTITY.md). Existing Libgcrypt/OpenSSL requirements remain. Actual Network attachment, registry/admission and complete login remain planned. Main and historical branches are preserved.


## alpha-0.2.8

**Current integration — alpha-0.2.8:** [Guest login](ADC_LOGIN.md) attaches the state/SUP/SID/INF/Tiger stack to the actual C++ TCP/TLS reactor with bounded ordered output, nickname/CID admission, public presence and complete ncdc NORMAL/reconnect tests. Historical stage descriptions below retain their original scope; statements that transport attachment/output/login were still planned describe those earlier versions. Registered-user authentication, chat, search and transfer dispatch remain planned.

CMake/npm version 0.2.8; runtime alpha-0.2.8. Branch alpha-0.2.8 starts from main b8978491fa8e146527eba818f9bc1ab0a31aa90d. No automatic main merge or release tag.


## alpha-0.2.9

CMake/npm 0.2.9; runtime/WebAdmin alpha-0.2.9. Base: main `da1521502c4a8936b3427a7d38fea6b904686b39`. [STA/QUI](ADC_STATUS.md) extends live guest sessions without a new API schema, dependencies or SQL migrations. No automatic merge/tag.


## alpha-0.2.10

CMake/npm 0.2.10, runtime/WebAdmin alpha-0.2.10. Base: main `a3ca70d65a0fcbb563147e8a44a1ce2669a0ec53`. [Security](NEGOTIATION_SECURITY.md) adds local negotiation policy without new dependencies, SQL migrations or API schema changes. No automatic main merge or tag.


## alpha-0.3.1

CMake/npm 0.3.1, runtime/WebAdmin alpha-0.3.1. Base: main `5bc79d0a12eb37d0869651eb48b57d2bf338d7fe`. [Session Core](SESSION_CORE.md) adds dc24h_users without new dependencies, migrations, security settings or API schema changes. No automatic main merge or tag.
