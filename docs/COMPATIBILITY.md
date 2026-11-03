# Compatibility and validation

Updated 2 November 2026 for beta 0.1.17.

## 0.1.17 dashboard and search update

All 9 editor suites pass. Windows Steam/Epic, Windows Server and Linux Server packages built successfully. The installed Windows client matches the tested package, and the owner confirmed the automatic search results look good in game.

This release also includes the 0.1.15 shaft snapping, darker dashboard focus and reserved-slot display enhancements. Full controller, all-context, joined-client, Epic and Linux runtime checks remain pending. The 307 native snap/display checks and 107 transport checks belong to the 0.1.15 candidate; they were not rerun for these UI-only search changes. See [current validation](VALIDATION.md).

## 0.1.14 build-menu fix

The Freight Elevators category now derives from the native build-menu category class. All 13 recipes are grouped under Item Freight, Fluid Freight or Shafts. Existing recipe and buildable paths are preserved.

| Check | Result |
|---|---|
| Regression with original category/descriptor assets | Failed as expected: wrong category type and no build subcategories |
| Regression with corrected assets | Passed for all 13 recipes, descriptor categories, subcategory types and product/buildable references |
| Editor automation | All 5 suites passed; 0 failed |
| Packaging | Windows Steam/Epic, Windows Server and Linux Server built successfully |
| Installed Windows client | Every packaged file matched the installed file; game log confirms 0.1.14 |
| Existing Steam single-player save | Loaded successfully on CL 513885, SML 3.12.0 |
| Native build-menu visual check | Owner confirmed the elevators appear in the menu on the existing save; a separate agent visual sweep of every entry was not completed |

The editor checks read serialized defaults because several static game descriptor getters are placeholders in the mod SDK. They do not substitute for the in-game UI check.

No new dedicated-server gameplay, Epic runtime, Linux runtime or joined-client test has been run for 0.1.14. The transport implementation is unchanged. The evidence below belongs to earlier builds and is not a fresh pass for this version.

## Historical 0.1.13 coverage

- Windows Steam gameplay, reservation UI, delivery, save and reload passed.
- Four editor suites and 107 Windows dedicated-server integration checks passed, including 13 outgoing-reservation checks.
- Windows Steam/Epic, Windows Server and Linux Server packages built. Epic and Linux runtime were not tested.
- Earlier builds passed client checks; the latest outgoing-reservation RPC/UI round trip still needs a fresh joined-client test.

Detailed historical evidence remains in validation/server.txt, live-repair.txt, reload.txt and HISTORICAL-VALIDATION.txt. Current editor evidence is in validation/editor-summary.json and build-menu-regression.json.

## Compatibility limits

The descriptor requires game build >=513885 and SML ^3.12.0. A minimum-version declaration does not guarantee future updates. Experimental/newer builds and console editions are not verified; this is a PC mod package.

Install the same version on every client and server. Powered Foundations is optional and separate.

Before calling the mod stable, test outgoing reservations through a joined client, reconnect and persistence; run item/fluid transport on Linux Server; and smoke-test Epic. Successful packaging is not runtime coverage.

The source manifest records the implementation and uncooked assets included in the matching source archive. Private saves, credentials and unredacted logs are excluded.
