# Player guide

## Install and unlock

For the GitHub beta, follow the [installation instructions](INSTALLATION.md). Satisfactory Mod Repository publication is still pending; once available there, install **Trapper's Freight Elevators** through Satisfactory Mod Manager. Install the same mod version on clients and server. Make a backup before updating an important factory.

Complete the Freight Elevators HUB milestones: Mk.1 at tier 4, Mk.2 at tier 6, Mk.3 at tier 8. Open the build menu with **Q**, select **Freight Elevators**, then browse **Item Freight**, **Fluid Freight** or **Shafts**. Search for **Freight** also works.

## Build a tower

1. Select an item or fluid elevator and place its base on an 8 m × 8 m area.
2. Click to anchor it, then aim upward or scroll to select up to ten additional 4 m sections. Click again to confirm. The base alone is 8 m tall.
3. Connect power to the drive.
4. Add matching floor stops where needed, including a stop at the base if you want ground-floor service.
5. Press **R** during stop placement to choose 1 m, 2 m or 4 m height snapping. This adjusts alignment with foundations; it does not allow overlapping stops. Keep at least 8 m between stops and 8 m of shaft above the highest stop.
6. Extend later with the same elevator tool aimed at the existing shaft. Each placement adds up to ten sections. The total tower limit is 256 m, including headroom.

A stop is a separate building, even when it overlaps the base visually.

## Connect belts and pipes

Open the panel using the normal Use key. Select the floor, then **Ports**.

Match the on-screen number to the plaque above the physical connection. Set each port to INPUT or OUTPUT before connecting it. Disconnect belts before reversing direction. Drain and disconnect fluid plumbing before reassigning a compartment.

- **INPUT:** receives material from this floor into its incoming storage.
- **OUTPUT:** sends delivered material from this floor's outgoing storage onto a belt or pipe.

Use output item filters to separate mixed deliveries onto dedicated belts. Fluid ports each serve one assigned compartment.

## Understand the three inventories

Every floor has its own two buffers:

- **Incoming:** waiting to travel from this floor to another floor.
- **Outgoing:** delivered to this floor, waiting to leave through its output ports.

**Onboard** is the shared moving cabin inventory, displayed read-only. Selecting another floor changes which floor buffers you are viewing.

Belts may fill incoming storage and drain outgoing storage while the cabin is elsewhere. They back up when the relevant buffer is full. The cabin transfers cargo only while docked with gates open.

## Route items

In **Routes**, choose a resource, destination floor and delivery weight. Add another destination for the same resource to divide its deliveries.

Example: 100 copper ingots/min for two destination floors:

| Destination | Weight | Target share when both accept cargo |
|---|---:|---:|
| Floor 2 | 1 | 50% |
| Floor 3 | 1 | 50% |

Weights 2 and 2 also split evenly. Weights 2 and 1 target two thirds and one third. Weights are relative shares, not items-per-minute limits. Travel time, belt capacity and blocked destinations affect observed delivery.

The elevator reserves destination capacity before picking cargo up. A full or disabled destination is skipped for new assignments. Already assigned cargo keeps its destination.

## Reserve item storage

Open **Reservations**, select a floor and either **Incoming** or **Outgoing**, choose an item, enter reserved stacks and save.

Stored stacks count toward the reservation. Items may use shared space beyond their own reservation, but cannot take another item's unused reserved stacks. A partially filled stack still uses one slot.

For example, in a 30-slot buffer, reserving 5 for rods and 10 for plates leaves 15 shared slots. Rods can use up to 20 slots when plates are absent, preserving 10 for plates.

Rules apply only to the selected floor and buffer. To guarantee rods can arrive at Floor 4, reserve rod stacks in **Floor 4 → Outgoing**. An incoming reservation on another floor does not reserve destination space.

Changing a rule never deletes existing cargo. If storage is already full, the reserved space becomes available as items leave. Outgoing rules also account for cargo already assigned to that floor. Remove a rule or save zero to release its reservation.

## Pickup and fluids

Immediate pickup ships the cargo present when the elevator arrives; new arrivals wait for a later trip. Each floor can instead wait until full with a maximum wait of 1–600 seconds. Default maximum wait is 30 seconds.

For fluids, choose one, two or four equal isolated compartments, assign their fluids, and then attach pipes. Change compartment count only with an empty system. A flush is destructive and applies to the selected floor's tanks and port contents.

## Tier specifications

| Specification | Mk.1 | Mk.2 | Mk.3 |
|---|---:|---:|---:|
| Item cabin / each floor buffer | 24 stacks | 48 stacks | 96 stacks |
| Fluid cabin | 300 m³ | 800 m³ | 1,500 m³ |
| Maximum stops | 6 | 12 | 20 |
| Cruising speed | 4 m/s | 8 m/s | 12 m/s |
| Active power | 10 MW | 25 MW | 50 MW |
| Idle power | 1 MW | 2 MW | 4 MW |
| Internal item transfer | 1,200/min | 3,600/min | 7,200/min |
| Internal fluid transfer | 600 m³/min | 1,800 m³/min | 3,600 m³/min |

Internal rates are shared across transfers at the dock. Travel, acceleration, gate movement and external belt/pipe capacity reduce sustained throughput.

## Dismantle safely

Empty the floor buffers and cabin first. Remove stops, then unused shaft sections from the top down, then the base. Aim at the numbered port until the tool identifies the freight stop if it overlaps the base.

For mass dismantle, hold Left Ctrl while selecting the parts, release Ctrl, then hold the left mouse button. Include supporting dependencies such as upper shaft sections and attached stops. Unselected dependencies and stored cargo prevent removal; nothing outside your selection is automatically dismantled.

## Troubleshooting

**No deliveries:** check power, pause state, the destination route, enabled floors, output space, reservations, port direction and output filter.

**Rods never reach a mixed-storage destination:** reserve rods in that destination's Outgoing storage. Existing full storage must drain before the reservation frees space.

**Belt will not connect:** connect to a numbered stop port, not the shaft or drive structure. Set direction first.

**Cannot dismantle:** inspect the on-screen reason, empty storage, and remove or select dependent stops and upper shaft sections.

**Power keeps tripping:** provide enough generation for active demand. Resetting the fuse does not add generating capacity.
