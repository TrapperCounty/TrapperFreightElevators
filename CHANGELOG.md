# Release notes

## 0.1.15 - dashboard and shaft snapping (unreleased candidate)

- Snap flat foundations and straight 8 m walls to all four shaft faces on a 1 m height grid. Foundation tops align to landing heights; snapping leaves stop openings and the drive controls clear. Rotated towers and Powered Foundations use the same alignment.
- Darker dashboard selected, hover and pressed states, with a visible keyboard/controller focus outline.
- Blue reserved slots in both storage buffers, faint item icons for empty reservations, and amber WAIT markers where existing cargo still occupies the allocation. Displayed stacks retain their original transfer targets.
- Type to filter the shared item/fluid picker in Ports, Routes, Reservations and Fluid tanks. Search preserves the selected item, including when it is filtered out. Clear restores the full list.

No saved inventory, routing rules or buildable classes are migrated. This candidate has not been published. See [validation](docs/VALIDATION.md) for test coverage.

## 0.1.14 - build-menu category fix

Addresses [issue #1](https://github.com/TrapperCounty/TrapperFreightElevators/issues/1): unlocked freight recipes appeared in search but were missing when browsing build categories.

- Registers Freight Elevators with the native build-menu category type.
- Groups all 13 recipes under Item Freight, Fluid Freight and Shafts.
- Preserves existing recipe/buildable paths and milestone unlocks.
- Adds an editor regression test that fails on the original assets and passes after the fix.

All five editor suites pass. Windows client, Windows Server and Linux Server packages built successfully. See the current [validation record](docs/VALIDATION.md) for runtime coverage and remaining checks.

## 0.1.13 — initial public beta candidate

Freight elevators for multi-floor factories, with item and fluid transport, extendable shafts and configurable stops.

- Three item and fluid tiers, four numbered configurable ports per stop.
- Build a base and shaft together; extend in groups of up to ten sections.
- Stop-height alignment in 1 m, 2 m and 4 m increments.
- Weighted routing to multiple destinations.
- Per-floor incoming and outgoing storage reservations.
- Isolated fluid compartments, per-floor wait settings and animated gates.
- Native mass dismantle with cargo and dependency protection.

### Latest fix

Outgoing storage now has its own reservations. Reserving rod stacks at a destination protects room for rod deliveries instead of only reserving source storage. Capacity calculations include cargo in transit, and unloading rechecks the current rules.

Existing incoming reservations persist when upgrading. Outgoing reservations start empty until configured. Changing a reservation never removes existing cargo.

### Validation

Four editor suites and 107 Windows dedicated-server integration checks passed for this build. Windows Steam gameplay, the reservation panel, normal rod delivery, save and reload were verified. Windows Steam/Epic, Windows Server and Linux Server packages built successfully.

Epic gameplay, Linux Server runtime and the latest outgoing-reservation behavior through a remote multiplayer client still need confirmation. Treat this as beta software; see the compatibility record.
