# Development notes from the release workspace

Historical notes below describe the local test harnesses. Workstation paths have been replaced by placeholders. Use BUILDING.md for portable setup.

# Trapper's Freight Elevators

A Satisfactory freight elevator mod for TrapperCounty. Private beta 0.1.13.

## Dismantling a tower

Version 0.1.12 explains blocked dismantles on the native HUD. A ground-floor
stop is separate from the base even though they overlap visually. Aim at the
numbered port until the tool names the freight stop, then remove the empty
stop before its base, or include both in a mass selection.


Version 0.1.11 supports native mass dismantle. Hold Left Ctrl while aiming at
each part to add it to the selection, then release Ctrl and hold the left mouse
button to dismantle the group. Select every supporting dependency you intend
to remove, including upper shaft sections and attached stops. The game orders
the selected parts automatically. Unselected supports and stored cargo remain
protected; nothing outside your selection is automatically removed.


Empty and remove its stops, then dismantle shaft sections from the top down.
Remove the base after the last shaft section is gone and its cabin is empty.
Version 0.1.10 fixes shaft targeting: the full-height player barrier no longer
causes the dismantle tool to select the base when aiming at a shaft section.
Existing towers receive the fix when loaded.

## Reserved item stacks

Open an item elevator, choose **Reservations**, then select the floor, **Incoming** or **Outgoing**, item and
number of **Reserved stacks**. Save reservation sets that item's guarantee;
saving it again replaces the count. Remove (or save zero) releases the guarantee.

Incoming and outgoing reservations are separate for each floor. Existing stacks count toward
the guarantee. Items may use the shared space beyond their reservation, while
other items cannot occupy their unused reserved slots. For example, in 30 slots,
reserving 5 for iron and 10 for copper leaves 15 shared: iron may occupy up to 20
slots while copper is absent, leaving copper its 10. A partial stack uses one
slot and can still be topped up without consuming another reserved slot.

The total reservation cannot exceed capacity. Changing a rule never deletes or
moves cargo; if storage is already full, protected space becomes available as
cargo leaves. Belts back up before taking an item that would consume protected
space, and manual deposits leave excess in the player's inventory. Reservations
persist in saves and are controlled by the server in multiplayer. They do not
change route shares, onboard capacity or fluid tanks. Outgoing reservations also count cargo already on the way, prevent conflicting pickups, and protect slots during unloading. Existing saves retain their incoming rules; outgoing rules start empty.

## Build a freight elevator

1. Select the item or fluid freight elevator and tier you want. Aim at a foundation
   and click to anchor the drive.
2. Aim upward or scroll to add up to ten 4 m shaft sections. Click again to build
   the drive and shaft together. For the 8 m base alone, click again at the
   starting height. Right-click to cancel the anchored preview.
3. Add matching freight stops where you want service. The base stop is optional.
   Use R to choose 1 m, 2 m or 4 m snapping. Leave 8 m between stops and 8 m of headroom.
4. To extend a matching tower, select the same elevator tool and aim at the shaft.
   Click, choose the added height, and click to build. Extensions cost only the
   selected shaft sections. A higher-tier elevator aimed directly at the base
   upgrades the drive. The existing standalone shaft tool also remains available.

The initial drive is 8m tall; maximum total tower height remains 256m. Material
cost scales with the selected number of sections. Each section is a normal saved
buildable with its own material refund. Removing a stop does not shorten the
shaft. Dismantle unused shaft sections from the top down; remove dependent stops
before dismantling their supporting structure.

Existing elevators keep their current height, stops, cargo and settings. Their
previous automatic shaft becomes a preserved part of the tower and can be
extended using the matching elevator tool. New floors require enough shaft to be built
first. Item and fluid towers share the shaft recipe. Landing screens open around
attached stops, and the hoist head and cables follow the constructed tower height.

## Ironworks appearance

The item and fluid elevators share a graphite frame with orange armor, open expanded-metal
shaft screens, reinforced docking collars, paired sliding safety gates, a
mechanical headhouse and an animated cable drum. The moving cargo cage shows
freight crates while loaded; fluid cages show one, two or four pressure vessels
to match their configured compartments. Screens leave the floor docking areas
open. All tiers retain the existing 8m footprint and native port positions.
Existing elevators acquire the new appearance when the save is loaded.
The refined Ironworks design adds layered structural covers, diagonal portal
shoulders, shaped storage housings with recessed loading mouths, sculpted hoist
covers, smoother cylinder shading and restrained brushed-metal surfaces.
The fluid gauges sit above the stationary storage so low fill levels remain visible.

Cabins now accelerate and brake into each landing. The listed tier speed is
cruising speed; short trips and opening/closing gates add to journey time.
Paired cabin and landing gates ease through an approximately 0.85-second stroke.
Cargo transfer begins only after opening, and travel waits until closing finishes.
Door and trip progress survive saves; pause and power loss halt the sequence.
Fluid vessel gauges smoothly follow the actual compartment fill level.

Armored floor-buffer housings make the stationary storage visible at each port.
Incoming belts can fill that storage while the cabin is away, and outgoing belts
can drain previously delivered cargo. A full input buffer backs up its belt.
The gate interlock applies to transfers between the buffer and cabin.

## Storage and port controls

The panel opens on Storage, with familiar item icons, gray slots and orange
stack counts beside your player inventory. Choose Incoming, Outgoing or Onboard.
Click a floor-storage stack to take it, or use Take all. Click a player-inventory
stack to deposit it into Incoming storage. Only available space is filled;
anything that does not fit stays at its source. Onboard cargo is read-only.
Fluid storage uses separate fill bars and cubic-metre counts for each tank.

Ports are numbered 1â€“4, with physical number plaques and INPUT/OUTPUT labels
above each entrance. Select the matching number in Ports to change direction,
item filter or fluid compartment. Pipe entrances use recessed round sockets.
Freight machinery now blocks player movement through the drive, stops and shaft.

## Playing

The current factory review save is **<private-save>**. The earlier
**<private-save>** contains the prepared playground; the original
**<private-save>** and earlier Ironworks review are preserved. The playground contains an elevated
three-floor platform with a Mk2 item elevator, a Mk2 fluid elevator, return belts,
separate water/oil pipe loops and four ordinary fueled biomass generators.
The playground is connected to the existing factory's ordinary power grid.
Use this separate save to experiment. The original factory save is preserved.

Unlock Freight Elevators Mk.1 / Mk.2 / Mk.3 through the HUB at tiers 4 / 6 / 8.
The **Freight Elevators** build-menu category contains separate item and fluid
drives and matching floor stops, plus a shared 4m shaft section. Each piece has
its own icon. You can also search for **Freight** in the build menu.

1. Anchor the item or fluid elevator on its 8 m × 8 m footprint, select the shaft
   height, confirm, and connect the drive's power terminal.
2. Add matching stops on the tower at the desired factory floors.
3. Press the normal Use key while looking at a stop or drive to open its panel.
4. Configure each stop's four ports as input or output, then connect belts/pipes.
   Disconnect a connection before reversing it. Fluid ports must also be drained
   before assigning them to another compartment.
5. Under Routes, select a resource, destination and delivery weight. Add another
   destination for the same resource to share deliveries. Equal weights split
   evenly; weights 7 and 3 target 70% and 30% by quantity.
6. For a fluid elevator, choose one, two or four equal compartments and assign
   their fluids before attaching pipes. Each port serves one compartment.

Each stop has separate incoming and outgoing buffers, each holding one cabin
load. Ports share those buffers. Outputs may have item filters. Floor buffers
remain available while the cabin serves other stops.

Immediate pickup is the default: the cabin loads only the quantities present
when it arrives. New arrivals wait for another visit. Each floor can instead
wait until full, with a configurable maximum of 1â€“600 seconds (default 30).
Depart now finishes unloading and skips the remaining loading wait.
The maximum fill-wait timer starts once the gates have opened; the arrival
snapshot is still captured when the cabin reaches the floor.

Cargo reserves destination space before pickup. Full or disabled destinations
are skipped; unmatched cargo remains at its source. Existing cargo keeps its
destination when rules change. Disabling a floor stops new assignments, while
already reserved deliveries may finish unloading.

## Tiers

| Specification | Mk1 | Mk2 | Mk3 |
|---|---:|---:|---:|
| Item cabin | 24 stacks | 48 stacks | 96 stacks |
| Fluid cabin | 300 mÂ³ | 800 mÂ³ | 1,500 mÂ³ |
| Maximum stops | 6 | 12 | 20 |
| Travel speed | 4 m/s | 8 m/s | 12 m/s |
| Active power | 10 MW | 25 MW | 50 MW |
| Idle power | 1 MW | 2 MW | 4 MW |
| Internal item transfer | 1,200/min | 3,600/min | 7,200/min |
| Internal fluid transfer | 600 mÂ³/min | 1,800 mÂ³/min | 3,600 mÂ³/min |

Internal rates are shared across the docked cabin's transfers. Actual sustained
delivery depends on belts/pipes, travel distance, stop count and loading time.
The footprint is shared by all tiers. The maximum shaft envelope is 256m,
including 8m above the highest stop; maximum usable stop-to-stop travel is 248m.

## Controls and cargo protection

The panel provides floor names, enable/disable, pickup mode, timeout, per-port
direction and filters, routing weights, fluid assignments, pause, depart now,
and native power-circuit fuse reset. Resetting a fuse cannot fix insufficient
generation: ensure the circuit can supply the active demand first.
The elevator releases its demand during a blackout so ordinary fuel generators
can restart, and supports native battery power and the game's No Power setting.

Empty and disconnect the relevant fluid plumbing before changing its fluid.
Changing the compartment count requires an empty system. Flushing is an explicit
two-step per-floor action and discards only that floor's tanks and port contents.

Collect item buffers into your inventory through the floor panel. Anything that
does not fit remains in the buffer. Occupied or reserved stops cannot be
dismantled. Remove floor stops before dismantling their drive.

Higher-tier drive holograms can upgrade a matching lower-tier drive. Cargo,
routes, buffers, cabin position, stops and power connections transfer to the new
drive. Blueprint copies retain configuration but start without cargo.

## Development

Workspace: `<modding-root>/SML/Mods/TrapperFreightElevators`

Build helper: `<modding-root>/Setup/Build-FreightElevators.ps1`

The helper compiles editor modules, generates assets with `TFGenerateAssets`,
optionally runs the editor specification suite, and packages selected platforms.
Targets: Steam, Clients (Steam/Epic), WindowsServer, LinuxServer, All.
The Linux v25 compiler is local to this workspace; the helper sets its environment
only for the build process. No global compiler setting is required.

Opt-in development flags:

- `-TFIntegrationTest`: isolated packaged-world test fixtures; no world save.
- `-TFShowcase`: prepares a separate Freight Elevators Playground session and
  save, with demonstration cargo and ordinary fueled generators.
- `-TFClientTest`: authenticated encrypted loopback multiplayer control checks.
- `-TFInspect`: read-only native power diagnostics for a loaded test world.
- `-TFPlaygroundReview`: reload/circulation checks in the separate playground.
- `-TFReady`: positions the player on the playground and opens the normal pause
  menu after loading, without creating fixtures.
- `-TFIronworksReview`: checks loaded visual components and keeps review saves
  separate from the original playground.
- `-TFUsabilityPhotos`: captures inventory, item/fluid port controls and a pipe socket. An unconnected fluid stop is temporarily spawned for the socket image and destroyed afterward. No save is made by this capture helper.
- `-TFIronworksPhotos`: with IronworksReview, captures native item/fluid/hoist
  camera views and checks that closing the freight panel releases player input.

Neither fixture runs in a normal launch. Use disposable saves for development.
Validation evidence and exact coverage are maintained in
`<modding-root>/QA/FreightElevators`.

Built for the local Satisfactory CL 513885 / SML 3.12.0 / custom UE 5.6.1-CSS
environment. Platform package builds and gameplay validation are different;
consult the validation record for which environments were actually run.
