# Install the GitHub beta

Download from the [0.1.17 release assets](https://github.com/TrapperCounty/TrapperFreightElevators/releases/tag/v0.1.17).

| File | Use |
|---|---|
| TrapperFreightElevators-0.1.17-Windows.zip | Windows game client, Steam or Epic |
| TrapperFreightElevators-0.1.17-WindowsServer.zip | Windows dedicated server |
| TrapperFreightElevators-0.1.17-LinuxServer.zip | Linux dedicated server |
| TrapperFreightElevators-0.1.17.zip | Combined archive containing all three targets; intended for SMR upload |
| TrapperFreightElevators-0.1.17-Source.zip | Complete development source and uncooked assets; not an installable mod |
| SHA256SUMS.txt | Checksums for all five ZIP downloads |

GitHub's automatic Source code archives and Code → Download ZIP do not contain the compiled mod. Download a named platform ZIP from Assets.

## Windows client

1. Back up your save and close the game.
2. Install Satisfactory Mod Loader compatible with your game, normally through Satisfactory Mod Manager. This beta was tested with game build 513885 and SML 3.12.0; it requires SML ^3.12.0. See the [compatibility record](COMPATIBILITY.md) before using a newer game build.
3. Create the folder `<Satisfactory install>/FactoryGame/Mods/TrapperFreightElevators/`.
4. Extract the **contents** of the Windows ZIP directly into that folder. The resulting descriptor path must be `<Satisfactory install>/FactoryGame/Mods/TrapperFreightElevators/TrapperFreightElevators.uplugin`, with Binaries and Content beside it. Do not add another Windows or TrapperFreightElevators directory inside it.
5. Start the game and verify Trapper's Freight Elevators 0.1.17 appears in the loaded mod list.

The combined archive has Windows, WindowsServer and LinuxServer folders at its root. If using that archive, extract only the contents of the appropriate target folder into the mod folder above. The platform ZIP is simpler.

Once published to Satisfactory Mod Repository, use Mod Manager for normal installation and updates. For now this is a manual GitHub beta download.

## Dedicated servers

Stop the server and back up its saves. Install a compatible SML, then extract the matching server ZIP into `<server install>/FactoryGame/Mods/TrapperFreightElevators/`, keeping the same descriptor/Binaries/Content structure. Install version 0.1.17 on all connecting clients too.

Matching server packages build successfully. The 0.1.15 candidate passed Windows dedicated-server transport and snapping fixtures; those gameplay suites were not rerun for the 0.1.16/0.1.17 search changes. Linux Server gameplay, Epic client gameplay and a fresh joined-client test remain unverified.

## Verify the download

Compare the ZIP's SHA-256 hash with SHA256SUMS.txt from the same release. In PowerShell, use `Get-FileHash -Algorithm SHA256 -LiteralPath 'path-to-downloaded-zip'`.

Folder placement and dependency handling follow the [Satisfactory modding manual-install guide](https://docs.ficsit.app/satisfactory-modding/latest/ManualInstallDirections.html).
