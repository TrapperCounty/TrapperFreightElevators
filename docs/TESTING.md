# Tester checklist

## 0.1.16 search regression

- In Routes and Ports, type a full query such as `wire` without re-clicking the field. The query and match count should update after every character; the popup should stay closed until clicked or opened with Down/Enter.
- Backspace, select/replace the query, and enter mixed-case or unmatched text. Click the list or press Down, then choose a result with the mouse or keyboard/controller. Filtering alone must never change the selected resource.
- Use Clear and Escape from the search field. Both the visible text and filter must reset, all items must return, and the previous selection must remain selected.
- Repeat in Reservations and Fluid tanks, which share the same search implementation. Check white text on the dark field, including while focused, and the amber focus border.

## 0.1.15 candidate checks

- Snap 1 m, 2 m and 4 m foundations and Powered Foundations to each side of a rotated tower, including manual shaft sections. Check top alignment at a landing, extend the shaft, then save/reload and repeat. Test straight 8 m walls between stops and confirm opening/control-panel heights are not offered as snaps.
- Navigate dashboard tabs and buttons with mouse, keyboard and controller. Confirm selected, hover, pressed and focus states remain distinct after settings refresh.
- Reserve four stacks independently in Incoming and Outgoing. Check four blue tiles when empty, partially filled and full; occupied conflicts should show WAIT. Remove/change the rule, switch floors and repeat from a joined client. Click occupied tiles and verify the correct stack transfers.
- Search by partial name and mixed case in Ports, Routes, Reservations and Fluid tanks. Try no matches, Clear, Down/Enter and Escape. Filtering must not silently change the chosen item. Check icons, popup readability and focus while typing.

Developers can run `-TFEnhancementTest` on a disposable dedicated server for native snap/placement and reservation-display fixtures. Powered Foundations compatibility cases run when that optional mod is loaded. This does not test player input, construction cost, controller navigation or joined-client interactions. `-TFIntegrationTest` runs the transport regression suite. Neither flag is needed for normal play.

## Published 0.1.14 checks

Install **0.1.14** using [INSTALLATION.md](INSTALLATION.md), then use a backed-up or separate test save. No Unreal/editor tools are needed to play-test. Use normal launch without developer QA flags.

For the 0.1.14 menu fix, check the Freight Elevators tab on both existing and new saves. Confirm unlocked item elevators/stops, fluid elevators/stops and the shaft appear in their groups without searching, and that selection opens the correct hologram. Locked tiers should remain hidden until unlocked.

Test item and fluid deliveries, incoming/outgoing reservations from both host and joined clients, multiple destination weights, interrupted trips and power loss, save/reconnect/reload, shaft extensions and protected mass dismantling.

For multiplayer, install the same mod version on the server and every client. Record whether an operation was performed by a host or joining client. Test both mods together only after checking each alone so a failure can be isolated.

The priority gaps are listed in [VALIDATION.md](VALIDATION.md). Do not treat packaged targets as gameplay-tested platforms.

## Report a result

Open [Issues](https://github.com/TrapperCounty/TrapperFreightElevators/issues/new/choose), choose Bug report, and include:

- Mod version, game build, SML version, platform and host/client role.
- Reproduction steps and expected versus actual behavior.
- Whether the issue survives saving/reloading and whether other mods are installed.
- Relevant screenshots and a small relevant log excerpt, after removing private information.

Windows logs are normally in `%LOCALAPPDATA%/FactoryGame/Saved/Logs/FactoryGame.log`. Saves are optional; do not post credentials, server join data or private saves publicly. A successful test report can be filed as an ordinary issue describing the environment and checks completed.

Developers can find opt-in test code in Source and historical harness instructions in DEVELOPMENT-NOTES.md. Some QA modes create fixtures, change inventory temporarily, or perform save-specific repairs. Those modes are not needed for normal testing.
