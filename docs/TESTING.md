# Tester checklist

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
