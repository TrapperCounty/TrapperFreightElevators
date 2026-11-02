# Compatibility and validation

Prepared 1 November 2026 for candidate 0.1.13.

| Environment or check | Evidence |
|---|---|
| Windows Steam, Satisfactory CL 513885, SML 3.12.0 | Live gameplay, reservation UI, delivery, save and reload passed |
| Editor automation | 4 suites passed, 0 failed |
| Windows dedicated-server integration | 107 checks passed, 0 failed, including 13 outgoing-reservation checks |
| Windows Steam and Epic binaries | Both packaged; Epic runtime not tested for 0.1.13 |
| Windows Server binary | Packaged and integration-tested |
| Linux Server binary | Packaged; runtime not tested for 0.1.13 |
| Remote multiplayer | Earlier builds passed client checks; outgoing-reservation RPC/UI round trip still needs a new client test |
| Experimental branch / newer game builds | Not verified |
| Console editions | Not supported by this PC mod package |

Server-side integration tests are not the same as joining from a remote client. Successful compilation is not a claim that a platform was exercised in gameplay.

The descriptor requires game build >=513885 and SML ^3.12.0. Use an SML release compatible with your installed game. The minimum-version declaration does not guarantee future game updates.

Install the same version on all clients and the server. Powered Foundations is optional and is not bundled.

## Before calling this a fully verified stable release

- Test outgoing reservations from a joined client: save, remove, invalid count, floor/buffer independence, reconnect and persistence.
- Run item and isolated-fluid transport on a Linux dedicated server.
- Launch and smoke-test the Epic client.
- Check the actual target game build before setting repository compatibility badges.

The candidate may be reviewed as a beta with these limitations stated openly.


The mod implementation and uncooked assets published here match the archived 0.1.13 source release byte for byte (see SOURCE-MANIFEST.json). Publishing source did not rebuild or rerun gameplay tests. Detailed redacted evidence is in validation/.
