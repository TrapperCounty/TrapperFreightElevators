# Compatibility and validation

Updated 2 November 2026 for the unpublished 0.1.17 automatic-results candidate.

## 0.1.17 automatic search results

Search now renders matching rows inline in a scroll box capped at 220 Slate units, instead of automatically opening a combo popup. Nonempty queries hide the closed combo; selecting a result clears the query and returns to the full picker with that item selected. The search implementation is shared by Ports, Routes, Reservations and Fluid tanks.

All 9 editor suites pass. The routed Slate keyboard regression now uses temporary Wire/Cable descriptor names (restored afterward), checks matching rows on every character without focus loss, and selects Cable through Down/Enter. It also checks replacement/backspace, no-match messages, Clear/Escape and removal of obsolete result handlers. Windows Steam/Epic packaging succeeded. Evidence: `validation/automatic-search-editor.json`.

These are editor input tests, not live Satisfactory/controller or multiplayer confirmation. The manual checklist includes scrolling through many matches and checking all four dashboard contexts.

## 0.1.16 search input fix

Issue #3 is addressed by keeping the results popup closed during typing, explicitly resetting the filter when clearing, and routing Down/Enter and Escape before the editable text box consumes them. Search text has explicit normal/focused white foregrounds on a dark background.

All 9 editor suites pass, including a new Slate input regression. It sends character and key events through a host that forwards preview/bubble events to the panel, exercising mixed-case multi-character typing, select-all/replace, backspace, Clear, Escape, selection preservation and Down/Enter popup activation. The initial regression against 0.1.15 failed on popup auto-opening and Clear leaving stale filtering/selection indices. Evidence: `validation/search-input-editor.json`.

Windows Steam/Epic client packaging succeeded. No save format, assets, transport rules or RPCs changed. No fresh server-runtime, multiplayer or live Satisfactory UI/controller test was performed. The fix has not been installed into the running player game; use the 0.1.16 checklist in [TESTING.md](TESTING.md) for that confirmation.

## 0.1.15 enhancements

Tested with Satisfactory CL 513885, SML 3.12.0 and Powered Foundations 0.1.4. No assets, inventory contents, save properties or existing buildable classes are migrated.

| Check | Result |
|---|---|
| Editor automation | All 8 suites passed: existing 5 plus BuildingSnap, StorageDisplay and ResourceSearch |
| Storage display | Unique, complete mapping of real inventory indices; four-slot reservations; multiple resources; partial stacks; in-flight space; full-buffer conflicts; removal and capacity bounds |
| Search | Selection preserved when filtered out, restored when clearing search, and cleared only by explicit selection |
| Snap geometry | All four faces, rotated towers, 1/2/4 m foundations, rectangular rotation, wall alignment, opening/control-panel exclusion, shaft-height limits and inner rail normals |
| Windows dedicated-server transport | 107 checks passed, including native power, belts, fluids, routing and reservation accounting |
| Native snap/placement integration | 307 checks passed on a Windows dedicated server, including all four faces of automatic and manual shafts with standard and Powered Foundations |
| Packaging | Windows Steam/Epic, Windows Server and Linux Server candidates built successfully |

The server snap fixtures call native recipe holograms, hit validation, hooked snapping and placement geometry validation on automatic and manual shaft surfaces. They test standard and optional Powered Foundations. They do not test player input, construction cost, actual construction/save/reload, or joined-client replication of the new interactions.

Wall placement can retain the game's normal soft-clearance warning where it touches shaft framing. The final check verifies that these warnings allow construction; hard placement disqualifiers still fail. No clearance checks are suppressed. Earlier test-fixture failures counted the initial spawn flag and soft warnings as placement failures; the fixture now resets the reason list as native validation does, then checks placement and `CanConstruct()`.

This was pre-installation validation. The 0.1.15 candidate was subsequently installed and its source merged in PR #2; the owner's recording exposed search-input issue #3. Full controller navigation and transfers from reordered tiles still need a player-session check. Epic and Linux runtime are untested.

Source inspection confirmed host inventories and replicated client storage snapshots retain physical indices; display tiles send the mapped index and expected item class through the existing transfer RPC. The editor SDK contains placeholder inventory constructors/getters, so editor fixtures populate serialized fields directly; the server fixture also exercises real constructors and stack sizes.

Current evidence: `validation/enhancements-editor.json`, `validation/enhancements-runtime.txt` and `validation/enhancements-transport.txt`. The disposable QA server was stopped after testing; its existing save files were retained.

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
