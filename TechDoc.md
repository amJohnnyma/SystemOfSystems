# SystemOfSystems: Game Design & Technical Implementation Roadmap

**Working title:** Untitled Exploration Sandbox  
**Document version:** 0.2 - editable design draft  
**Prepared:** 17 November 2026  
**Updated:** 18 November 2026  
**Project:** [amJohnnyma/SystemOfSystems](https://github.com/amJohnnyma/SystemOfSystems)  
**Code baseline:** `main` at `9c14bb9` (16 November 2026)  
**Reference:** *Complete Systems R&D Technical Specification & Implementation Plan (v4.1)* (`Plan.pdf`)  
**Status:** Proposed, not approved. All recommendations and examples are open to revision.

> **Project philosophy: "When I make a system, make it properly."**
>
> In this document, *properly* means explicit ownership, reusable interfaces, data-driven definitions where useful, measurable performance, predictable behaviour, safe mutation and tests. It does **not** mean implementing every imaginable feature before the first playable game. Build each chosen system on a sound foundation, but keep the initial implementation bounded.

## How to use this document

- **Part I - Game design:** Player experience, gameplay loops, progression, and unresolved design choices.
- **Part II - Technical specification:** Systems, responsibilities, data contracts, performance and architectural alternatives.
- **Part III - Implementation roadmap:** Begin with **Step 1: Live 3D Player Presentation**, then continue the existing numbered phases from the present codebase. Each phase separates **native C++ work** from **Godot/GDExtension integration** and includes a completion gate.
- **Part IV - Decision register and scope:** Choices to make before implementing dependent systems, a vertical-slice target, and deferred features.
- `D-xx` denotes a **game-design decision**; `T-xx` denotes a **technical decision**. An option marked *lean* is a recommendation, **not** an accepted decision.
- Checkboxes `[ ]` are intentionally left open so implementation progress can be maintained here.

> **Immediate next task:** Complete **Part III, Step 1 — Live 3D Player Presentation** before resuming Phase 7. Only the player (and potentially exceptional bosses later) uses live 3D; normal entities keep their existing spritesheet/MultiMesh renderer.

---

# Part I - Game Design Specification

## 1. Game identity

### 1.1 High-level pitch

A single-player, side-view, procedurally generated **exploration-action sandbox**. The player ventures into dangerous biomes, discovers authored-but-procedurally-placed landmarks, fights enemies with tight animation-driven combat, acquires unusual resources and clues, and finds different ways to challenge powerful bosses. Loot is brought home to craft equipment and prepare for the next expedition.

The home base is a compact **expedition headquarters**: storage, useful NPC services and a small number of straightforward passive or input-to-output production buildings. Clearing and helping locations may unlock new services or allow limited reclamation. Major discoveries may provoke **infrequent, signposted retaliation**, with alternatives to defending the base directly.

**One-sentence player fantasy:** *Venture beyond safety, uncover secrets, survive difficult expeditions, return stronger, and gradually leave a meaningful mark on a living world.*

### 1.2 Design pillars

1. **Exploration is the primary activity.** Discovery should feel worthwhile even when it is not strictly required for progression.
2. **Combat rewards mastery.** Attacks, enemy telegraphs, timing, recovery, dodging and positioning should feel deliberate, responsive and fair.
3. **Multiple valid routes.** Quests, drops, puzzles and secrets can grant alternative ways to reach bosses or obtain important upgrades. The world is not one required quest chain.
4. **Structured discovery.** Hand-authored rooms, puzzles, enemies and story fragments are assembled and placed procedurally to produce variety without attempting fully generated narrative content.
5. **Home supports adventure.** NPCs, crafting and simple automation reduce friction between expeditions; they must not become mandatory micromanagement.
6. **Actions have consequences.** Cleared landmarks, recruited NPCs and rare boss milestones can change services, access, encounter pools or occasional world events.

### 1.3 Explicit non-goals for the first playable release

- Not a civilization/colony simulator; no individual citizen needs, occupations and full labour AI.
- Not a full RTS; no marquee-selected armies or broad unit-command HUD unless later design demands one.
- Not a Noita-like pixel/fluid simulator; terrain interaction remains tied to the existing density-grid engine.
- No multiplayer or networking requirement.
- No fully procedural dialogue, unlimited quest grammar or emergent political simulation.
- No infinite content requirement. A smaller set of polished enemies, structures and bosses is preferable.
- No obligation to support 10,000 fully simulated combat-capable agents near the player. Capacity for lightweight entities is a separate performance objective.

## 2. Player goals and progression

### 2.1 Immediate, medium and long-term goals

| Horizon | Player question | Typical reward |
| --- | --- | --- |
| Moment-to-moment | Can I survive this fight or reach that passage? | Health, positioning, loot, access |
| Expedition | Can I recover valuable materials, a clue or a key and return? | Resources, information, a new point of interest |
| Region | Can I understand a region and challenge its major threat? | Equipment tier, ability, region change |
| World | Which threats, secrets and bosses do I tackle, and in what order? | Long-term progression and world state |
| Home | How do I make the next expedition easier? | Crafting services, stockpiles, optional upgrades |

### 2.2 Open-ended sandbox versus authored conclusion

The exploration world remains persistent and partially non-linear. Boss encounters and authored milestones provide direction. A final objective may eventually exist, but reaching it should not require clearing every structure or completing every side quest.

**D-01 - Overall progression shape**

| Option | Benefit | Cost / risk |
| --- | --- | --- |
| A. Soft region tiers *(lean)* | Allows skipping and sequence-breaking while making danger readable | Requires careful difficulty signals and alternative access paths |
| B. Fully open from the start | Maximum freedom | Weak pacing; players may wander into content far beyond their capability |
| C. Explicit gated chapters | Easier balancing and teaching | Works against the intended sandbox freedom |

**Unresolved:** Do major bosses unlock biomes, enable technology, change the world, or only provide powerful optional rewards? Avoid hard-gating every region by one boss drop.

## 3. Primary gameplay loops

### 3.1 Expedition loop - the central loop

**Prepare -> Explore -> Fight / investigate -> Recover rewards -> Decide whether to continue -> Return -> Improve equipment -> Repeat.**

1. At home, select weapon, armour, tools, torches, ropes, consumables and optional summons.
2. Choose a region using visible landmarks, clues, rumours, map information or curiosity.
3. Traverse natural caves and placed points of interest (POIs). Mine selectively; the game should not demand constant bulk resource grinding.
4. Fight enemies whose attacks are readable and avoidable; gather materials, keys, clues or recipes.
5. Assess risk: continue with diminishing supplies, or retreat and bank the gains.
6. Return to base to craft, heal, store, trade or use simple production buildings.
7. Unlock fresh opportunities and repeat. Going home must be useful but not an excessive travel chore.

**Decisions and risks**

- **D-02 - Expedition pressure:** (A) Supplies, danger and distance only *(lean initially)*; (B) regional threat meter; (C) timed extraction. Do not add artificial timers before testing normal exploration.
- **D-03 - Return travel:** (A) player-built shortcuts / fast-travel anchors *(lean)*; (B) teleport consumable; (C) manual travel only. Travel friction should create choices, not repetitive walks.
- **D-04 - Death penalty:** (A) return to checkpoint and lose some carried resources; (B) recoverable backpack; (C) keep items, lose consumables/durability. Select after combat pacing tests; avoid punishing exploration into inactivity.
- **D-05 - Inventory friction:** limited slots, weight, or neither? The current slot-based inventory can support a limited-capacity test without a complex weight system.

### 3.2 Landmark / discovery loop

**Notice clue -> Find landmark -> Investigate -> Resolve encounter / quest / puzzle -> Gain information or access -> Identify another lead.**

- Landmark types: abandoned mineshaft, inhabited underground home, ruined fortress, shrine, watchtower, boss arena, rare underground laboratory.
- Each landmark must have a reason to exist: interesting fight, resource, secret, optional quest, shortcut, lore, service or boss route.
- Players can discover a boss lead before finding the quest giver or required structure.
- Landmark instances have persistent states such as `Unknown`, `Discovered`, `Active`, `Resolved`, `Claimed` or `Depleted`. Do not equate *visited* with *completed*.
- One region may offer several leads to a boss, with at least one reliable route and optionally one low-probability shortcut.

**D-06 - Landmark discovery guidance:** (A) subtle environmental clues *(lean)*; (B) map rumours / NPC hints; (C) explicit quest markers. Multiple channels can coexist, but exploration should not become a marker checklist.

### 3.3 Multiple-route boss-access loop

**Discover threat -> Find one of several access routes -> Obtain prerequisite / open path -> Choose when to engage -> Fight boss -> Carry reward or consequence forward.**

#### Example region: The Buried Warden

| Location / route | Example activity | What it grants | Required? |
| --- | --- | --- | --- |
| Abandoned mineshaft | Fight a rare mining creature | Fragment used to craft a summoning item | No |
| Underground hobbit home | Deliver 5 iron and either kill 5 goobers **or** provide a goober drop | Key to the fortress | No |
| Large ruined fortress | Defeat defenders and solve a compact switch/sigil puzzle | Opens an arena entrance | No |
| Hidden alternative | Find an unusual tunnel or obtain a rare enemy drop | Another route to the encounter | Optional discovery |

The fortress may also contain unique loot even if the player already crafted the summon elsewhere. **Alternate routes should not make the other structures worthless.**

**Boss access design constraints**

- At least one route must be reliably obtainable in every valid world seed. Never make a mandatory boss depend solely on an unbounded random drop.
- An alternative route may be lucky or difficult, but must be legible after discovery.
- Important access items should not be silently consumed without telling the player what they do.
- If a key is consumable, the player needs a repeatable way to obtain another or a permanently unlocked route.
- A player may approach a stronger boss early. Telegraph danger through enemies, terrain, rewards and the encounter rather than simply blocking entry.
- Boss unlocks should use a **dependency graph** with optional alternatives, not hard-coded `phase_number == n` checks.

**D-07 - Summoning model:** (A) reusable unlocked summon; (B) consumable crafted summon with renewable ingredients; (C) persistent arena activation *(lean for initial vertical slice)*. Repeat attempts must not force excessive farming.

### 3.4 Combat loop

**Read enemy -> Position -> Commit to attack / defend / evade -> Resolve active frames -> React to outcome -> Recover -> Repeat.**

The intended feel is inspired by Dead Cells, but possibly more deliberate, with fewer enemies at once. Player input should be responsive while actions retain meaningful commitment. **Player visuals use a live, rigged 3D model rendered into the 2D scene and pixelated with a shader**, while gameplay remains 2D and C++-authoritative. Ordinary enemies continue using the current MultiMesh/spritesheet renderer.

#### Core combat requirements

- Separate **physical collision bounds**, **visual representation bounds**, **hurtboxes** and **attack hitboxes**, regardless of how the character is drawn.
- Attack definitions specify startup, active and recovery periods, damage, knockback, optional movement modifiers, stamina/resource costs if selected, and cancel rules.
- Input buffering allows a pressed attack slightly before the current action can complete; buffer length is data-driven.
- During an attack, new horizontal input may be ignored or reduced while **existing velocity still updates through physics**; never freeze the entire motion state to impose an input lock.
- Player and NPC attacks use the same core hit/damage rules, with different controllers.
- A swing must not deal damage every physics frame unless deliberately configured. Track unique targets hit by each attack instance.
- Damage, hurt, invulnerability, knockback, death and item drops must have explicit state transitions.
- Enemies need readable telegraphs and punishable recoveries; effects, audio and brief visual feedback are part of the gameplay contract.
- Controller/mouse inputs should remain action-mapped for future rebinding.

**D-08 - Defensive move:** (A) dash with invulnerability frames *(lean for prototype)*; (B) block/parry; (C) both later. Start with one fully working defensive verb.

**D-09 - Combat complexity:** (A) one basic combo + dash + 2 enemy archetypes *(lean for vertical slice)*; (B) multiple weapons and stances; (C) equipment-driven animation-cancel systems. Ship feel before breadth.

**D-10 - Terrain in combat:** (A) mostly traversal/mining *(lean)*; (B) a few designated breakable arenas or attack effects; (C) systemic terrain-shaping combat. Keep C deferred unless it becomes a central player fantasy.

### 3.5 Loot, crafting and equipment loop

**Defeat / mine / discover -> Obtain resource or clue -> Learn recipes -> Craft / upgrade -> Equip -> Access harder content.**

- Distinguish common resources, rare equipment materials, consumables, quest items, boss access items, permanent unlocks and cosmetic/collectible rewards.
- New monster types should sometimes introduce meaningful resource categories, not just larger numbers.
- Recipe discovery may happen on pickup, at a crafting station, through NPC dialogue or by finding a blueprint; select one primary rule.
- The player should not need a wiki to discover a mandatory progression recipe.
- Loot from a hard boss must feel valuable even if a player already obtained a key or recipe another way.
- Crafting should favour small numbers of clear material dependencies over hundreds of nearly identical recipes initially.

**D-11 - Recipe discovery:** (A) auto-reveal recipes on obtaining ingredients *(lean)*; (B) NPC research / recipes; (C) experimentation. Can mix B for unique equipment and A for necessities.

### 3.6 Home headquarters loop

**Return with rewards -> Improve gear / recruit NPC -> Unlock a service or production building -> Prepare next expedition.**

The base should be useful even if the player builds only a small house. Optional building depth is welcome; mandatory micromanagement is not.

#### Candidate services

- **Storage:** organize resources, move items to crafting; choose whether crafting can read nearby storage automatically.
- **Blacksmith:** equipment and upgrade recipes.
- **Alchemist:** potions, special consumables and boss preparation.
- **Cartographer / researcher:** discovered POI hints and interpreted clues; avoid revealing the entire map by default.
- **Trader:** buy common supplies and sell selected valuables.
- **Production building:** passive resource output under a condition, or `inputs -> outputs` at a defined rate.

#### Simple production definition

Examples: `3 ore + 1 fuel -> 1 ingot per 10 s`; or a reclaimed mine yielding `1 ore per 20 s` while powered/secured. Production is **rate-based**, not a mandatory simulation of worker pathfinding.

**D-12 - Base services:** (A) NPC service unlocked by rescue; (B) crafted station only; (C) NPC + station requirement *(lean where it creates a clear expedition reward)*.

**D-13 - Automation:** (A) input -> output processing *(lean first)*; (B) passive yield from claimed POIs; (C) both. Define stockpile limits and activation conditions; avoid idle-game timers that force waiting.

**D-14 - Storage logistics:** (A) single shared settlement storage *(lean)*; (B) local chests with proximity rules; (C) transport/worker logistics. C is out of scope.

### 3.7 Retaliation and optional siege loop

**Major action -> Threat is signposted -> Player chooses response -> Consequences resolve -> Reward / world state changes.**

Retaliation should be **rare and tied to meaningful actions**, such as defeating a regional boss, taking a faction artifact or reclaiming a hostile fortress. It is not a periodic tax for owning a base.

#### Three response routes

1. **Defend:** prepare simple barriers/traps and fight at the headquarters.
2. **Intercept:** find and defeat the raiding leader before the attack reaches home.
3. **Ignore:** lose access to some optional services temporarily or later reclaim the area; avoid permanent destruction of irreplaceable stored equipment.

The response choices mirror the non-linear philosophy of boss progression. Players who prefer combat should not be forced into tower-defense design.

**D-15 - Retaliation model:** (A) one authored milestone encounter *(lean for vertical slice)*; (B) limited set of event templates; (C) dynamic faction/threat simulation. C is deferred.

**D-16 - Failure consequence:** (A) temporary merchant closure / occupied site *(lean)*; (B) damaged but repairable buildings; (C) full destruction/loot loss. Avoid C initially.

**D-17 - Optionality:** (A) retreat and intercept always possible *(lean)*; (B) occasional unavoidable home defense; (C) retaliation disabled until the player opts in. Evaluate through playtesting.

## 4. World and content structure

### 4.1 Content hierarchy

`World seed -> biome/region -> POI placement -> authored blueprint/room composition -> encounter/puzzle/loot variation -> persistent POI state`

Natural terrain remains procedural. The **interesting authored content** should be modular and parameterized: several mineshaft variants, an underground home, fortress rooms, encounter sockets and puzzle components.

#### POI generation invariants

- Each POI uses stable IDs and a deterministic world-seed-based random stream.
- Each blueprint declares biome/depth/size/access constraints, clearance and terrain stamping requirements.
- Placement must not overwrite existing protected structures, player edits, active entities or another reserved POI.
- Entrances and exits must connect to traversable space after stamping. Validate reachability, not merely free cells.
- The system must respect chunk boundaries and correctly dirty all affected chunks.
- Quest-linked and boss-linked POIs require world-generation guarantees: reserved placements, fallback variants or alternate valid routes.
- Randomize enemies, loot, rooms and decorations within authored constraints; avoid generating puzzles that cannot be solved.
- Store a POI's **instance seed and state** so reloading cannot reroll it.

### 4.2 Authored versus procedural divisions

| Author-driven | Generator-driven |
| --- | --- |
| Structure themes, room layouts, puzzle logic | Site selection, variant selection, room/loot socket rolls |
| Enemy archetypes and attack patterns | Encounter mix within a budget |
| Quest templates and rewards | Valid item/enemy/location parameter binding |
| Boss movesets and arena rules | Boss route placement and optional hints |
| Story fragments | Discovery order and optional connections |

### 4.3 Content budget for the first vertical slice

- One region with surface and caves; a second biome is optional.
- Three POI types: mineshaft, friendly underground home, fortress.
- Two or three variations of the mineshaft; one tested variant of the other POIs.
- Two normal enemy types plus one elite or boss.
- One boss with a small but polished moveset.
- Two quest objective templates and one composite quest.
- At least **two reliable routes** plus an optional lucky or hidden route to that boss.
- One recruitable service NPC and one production building.
- One optional, scripted retaliation encounter only after the main loop is fun.

## 5. Quality and player-experience principles

- **Combat first:** player movement and attack feel must work before content volume expands.
- **Honest danger signals:** difficulty is communicated by enemy behaviour, biome visuals and rewards. Avoid surprise one-shots as the primary warning.
- **Respect time:** repeat boss attempts should be short; travelling home must not erase the pleasure of exploration.
- **No hidden mandatory RNG:** chance can reward the lucky, but must not be the only guaranteed progression path.
- **Exploration without checklist exhaustion:** don't fill the world with obligatory markers and task counters.
- **Readable cause and effect:** why a siege is happening, how production works and why a boss can be summoned should be discoverable in game.
- **Accessibility considerations:** remappable actions, clear attack telegraphs, distinct hit feedback, scalable UI and an option to reduce intense effects.
- **Modularity:** combat, loot tables, POI definitions and quests should be data-driven within validated schemas; not everything needs a scripting language.

---

# Part II - Technical Specification and Architecture Choices

## 6. Current implementation baseline (read-only code review snapshot)

> **Scope note:** This is a planning snapshot, not a complete correctness audit or benchmark. It reflects the GitHub `main` revision `9c14bb9`, not uncommitted local work. The original PDF contains overlapping phase labels: one earlier outline calls item scattering *Phase 6*, while the detailed Godot-linked section labels entity architecture *Phase 6*. This roadmap follows the **detailed phase numbering** and the user's report that the entity phase is the last major completed milestone.

| Component | Observed status | Files / follow-up |
| --- | --- | --- |
| Density-grid terrain, dirty chunks and mesh generation | Substantial | `src/world_grid.*`, `src/marching_squares.*` |
| Configurable surface/underground generation | Substantial | `src/terrain_generator.*`, `game/game_settings.json` |
| Background jobs and parallel entity ticking | Substantial; needs invariant tests | `src/thread_pool.h`, `src/world_grid.cpp` |
| Structures and placement | Placement implemented; production tick incomplete | `src/structure.*`, `src/world_grid.cpp` |
| Entity simulation and movement | Substantial; temporary chase-to-player behaviour | `src/entity.*`, `src/entity_movement_manager.*` |
| Phase-6 spatial indexing | **Not complete as originally specified** | `src/entity_manager.h` contains comments, not an operational manager; per-cell entity occupancy is written on spawn but not maintained on movement |
| Rendering | MultiMesh entity buckets and existing player spritesheet visuals exist; player-only live 3D replacement is the new first task | `src/godot_world.cpp`, `demo/main.gd` |
| Input | Custom action names and input modes exist | `demo/input_manager.gd` |
| Player attack | Animation and per-frame attack hitboxes exist; damage integration incomplete | `src/player.cpp`, `game/player.json` |
| Health and enemy attacks | Incomplete | `Player::takeDamage`/`takeHealing` and `Entity::do_attack` have empty bodies |
| Item registry and inventory types | Defined; gameplay loop not yet connected | `src/item.h`, `src/inventory.h`, `src/inventory_manager.h` |
| Inventory UI | Temporary *item-definition browser*, not actual owned slots | `demo/inventory_ui.gd` |
| Dropped-item lifecycle, pathfinding, quests, POIs and persistence | Planned/not established as complete | New work below |

### 6.1 Immediate implications

1. **Complete spatial indexing before combat queries.** Stale per-cell memberships would produce incorrect hits/aggro even if initial spawns work.
2. **Do not treat render buckets as collision/query buckets.** Rendering uses state/type groups; combat needs a spatial broad phase.
3. **Separate intent from resolution.** Current entities move toward the player; future navigation and combat must choose targets, path, then move/attack through common services.
4. **Make item definitions actionable.** Existing item and inventory types are useful foundations; a real acquisition/use loop is still required.
5. **Keep a pure C++ authority boundary.** Godot provides input, asset loading, presentation, UI and debug views; C++ owns simulation and authoritative gameplay state.

## 7. Proposed system boundaries

| System | Authority / responsibilities | Must not own |
| --- | --- | --- |
| `WorldGrid` / terrain service | Cells, density, protected areas, edits, chunk dirtying, collision queries | Quest logic, NPC dialogue |
| `WorldSimulation` coordinator | Fixed-step orchestration, command/event routing, save snapshots, phase ordering | Every feature's internal algorithm |
| `EntityManager` | Stable entity IDs, creation/destruction, position updates, spatial index, cheap lookups | Boss rules, rendering resources |
| `MovementSystem` | Gravity, collision, stepping, movement constraints | Player input polling, quest progress |
| `NavigationSystem` | Traversal graph, route requests, path invalidation, steering targets | Direct damage application |
| `CombatSystem` | Attack state, hit detection, damage, knockback, death and combat events | Spawned UI labels, audio playback |
| `ItemSystem` | Item definitions, inventory changes, drops, pickups, equipment and use events | Godot inventory window layout |
| `StructureSystem` | Valid placements, instances, input/output production ticks, upgrades | Autonomous town simulation |
| `POISystem` | Blueprint placement, world instance IDs, generated encounter sockets, POI state | Procedural quest text generation |
| `QuestSystem` | Objective predicates, completion, rewards and event subscriptions | Polling every enemy and cell every frame |
| `ProgressionSystem` | Boss routes, unlock flags, prerequisite graph and content accessibility | Arbitrary hard-coded phase counters |
| `WorldEventSystem` | Milestone-triggered retaliation/event state | Dynamic political/faction simulation initially |
| Godot adapter + GDScript UI | Input mapping, assets, shaders, normal-entity MultiMesh, player-only live 3D presentation, sounds, effects, dialogue, menus and feedback | Authoritative inventory, damage or quest truth |

**Migration policy:** These are desired responsibilities, not a demand to immediately create twelve large classes. Extract systems incrementally as they gain real responsibilities. Keep legacy methods behind narrow adapters while converting call sites and tests.

## 8. Tick, data and event pipeline

### 8.1 Proposed simulation order

1. Collect input/UI commands into plain-data intent records.
2. At a fixed C++ simulation step, read the current world state and validate commands.
3. Update controllers, AI targets and navigation intent.
4. Update movement and resolve terrain collisions.
5. Synchronize moved entities into spatial buckets.
6. Advance attack animation/timing; perform broad-phase and narrow-phase hit tests.
7. Resolve damage, knockback, deaths and queued loot/structural changes.
8. Commit terrain/structure/entity mutations in a safe phase and mark changed chunks.
9. Consume events for quests, progression, inventory, audio and UI notifications.
10. Produce immutable/packed render data for Godot; generate dirty chunk meshes as appropriate.

**Concurrency rule:** Parallel jobs should work on disjoint entity state or read-only snapshots. Structural mutations, quest rewards, kills, spawn/despawn and bucket changes are collected as commands/events and committed deterministically at a synchronization point. Never mutate a container while another worker is traversing it.

### 8.2 Events versus continuous queries

| Event example | Consumers |
| --- | --- |
| `EntityKilled(killer, victim_type, location)` | Loot, kill objectives, progression, sound/VFX |
| `ItemCollected(actor, item, count)` | Inventory, collect objectives, UI |
| `ItemDelivered(actor, npc, item, count)` | Quest, inventory removal, NPC response |
| `POIDiscovered(poi_instance)` | Map, quest hints, lore |
| `POIResolved(poi_instance, resolution)` | Rewards, base unlocks, world state |
| `BossDefeated(boss_instance)` | Progression, recipes, optional retaliation |
| `StructureProductionCompleted(instance, output)` | Inventory/storage, effects, UI |
| `TerrainModified(chunk_ids)` | Mesh rebuild, navigation invalidation |

Events are **facts that happened**; commands are **requests to perform actions**. Event processing must not grant the same reward twice on reload or repeated callbacks. Use stable instance IDs and claim/completion state.

### 8.3 Stable IDs and ownership

- Keep `type_id` (template identity) separate from `instance_id` (one spawned instance).
- Avoid storing raw pointers to elements in containers that may reallocate or be erased.
- A stable handle may be a monotonically increasing integer for the first version; generation counters become useful if IDs are reused.
- Queries return immutable summaries or handles, and clients resolve handles at the moment they act.
- A destroyed entity must be removed from spatial buckets and all pending target links safely.

## 9. Technical alternatives: decide before committing to abstractions

### T-01 - Entity storage and spatial broad phase

| Option | Advantages | Disadvantages | Assessment |
| --- | --- | --- | --- |
| A. Keep `unordered_map<ID, Entity>` and add chunk `unordered_map<ChunkKey, vector<ID>>` | Minimal migration; expected average O(1) ID lookup and chunk lookup | Pointer chasing, allocations; must maintain bucket invariants | **Lean now** |
| B. Dense entity vectors + ID-to-index maps | Better cache locality and iteration | More complex erase/compaction; IDs need indirection | Reconsider with profiling |
| C. Full SoA/packed ECS | Maximum hot-loop control | Largest architecture change; high solo-dev cost | Defer |

Do not confuse average O(1) hash lookup with O(1) *complete area queries*: an area query still visits overlapping buckets and their candidate entities. Each moving entity must update its membership when crossing a bucket boundary. For larger hitboxes, either insert them into every overlapped bucket or expand query bounds appropriately.

### T-02 - Entity occupancy versus `Cell` layout

| Option | Advantages | Disadvantages | Assessment |
| --- | --- | --- | --- |
| A. Keep per-cell entity vectors, update them on every move | Simple exact occupancy checks | Large per-cell memory overhead, expensive maintenance | Not preferred at world scale |
| B. Separate chunk spatial index; query candidate AABBs | Reusable for attacks, aggro and culling; lower static cell overhead | Broad-phase false positives require AABB filtering | **Lean** |
| C. Dense local tile occupancy only for active areas | Fast narrow tile queries | Complex streaming/cache invalidation | Defer unless needed |

The current `Cell` includes a `std::vector` and structure ID, so the original document's four-bytes-per-cell estimate does **not** describe current memory usage. Measure memory before scaling to millions of cells; store sparse entity references outside terrain cells if possible.

### T-03 - Navigation in deformable platformer terrain

| Option | Advantages | Disadvantages | Assessment |
| --- | --- | --- | --- |
| A. Free-space 8-way tile A* | Familiar, straightforward | Finds routes that are geometrically open but impossible to walk/jump | Insufficient alone |
| B. Walkable-surface nodes + jump/fall edges, local A* | Matches actual movement capabilities | Edge generation and validation work | **Lean for gameplay** |
| C. Hierarchical chunk graph + local platform edges | Scales for distant targets | Requires cross-chunk portals and invalidation | Add once B works |

Build traversal edges from the entity's footprint, jump reach, safe falls and step height. Terrain changes invalidate only affected graph sections/chunks. Treat moving-target pursuit separately from static route planning; use direct steering only when the target is reachable, not merely visible.

### T-04 - Combat data representation

| Option | Advantages | Disadvantages | Assessment |
| --- | --- | --- | --- |
| A. JSON-defined attacks and frame windows interpreted by C++ | Reuses current data-driven animation approach; testable | Schema validation and tooling required | **Lean** |
| B. C++ subclasses per weapon/enemy | Easy custom logic | Scales poorly for content; code recompiles | Use only for exceptional mechanics |
| C. Generic behaviour scripting/visual editor | Flexible authoring | Large tooling project | Defer |

Prefer one `AttackDefinition` with explicit startup/active/recovery, hitbox sequences, damage, knockback, targeting rules, unique attack-instance ID and optional event hooks. Keep hitbox generation and successful-hit accounting separate from rendering frames.

### T-05 - POI construction

| Option | Advantages | Disadvantages | Assessment |
| --- | --- | --- | --- |
| A. Fixed authored blueprints with randomized spawns/loot | Fast, predictable, easy to validate | Layout repetition | **Lean for first 3 POIs** |
| B. Modular rooms with typed door/socket connections | Greater variety with reusable assets | Requires connectivity and puzzle validation | Second stage |
| C. Fully procedural structure grammar | Potentially enormous variety | High debugging/content cost, weaker authored pacing | Defer |

Keep a **blueprint format** and a **placement validator** even if the first implementation only selects whole blueprints; that is a proper extensible foundation without requiring the entire grammar immediately.

### T-06 - Quest and boss rules

| Option | Advantages | Disadvantages | Assessment |
| --- | --- | --- | --- |
| A. Authored predicates composed with `all_of`, `any_of` | Understandable, serializable, testable | Expressiveness is bounded | **Lean** |
| B. C++ quest class per quest | Arbitrary rules | Content and code become tightly coupled | Only for special encounters |
| C. Full scripting language for quest generation | Maximum flexibility | Tooling, security and debugging burden | Defer |

Subscribe quest objectives to domain events instead of repeatedly scanning world state. Distinguish objectives that count events **after acceptance** from those querying current inventory/achievement state; decide per objective type.

### T-07 - Production simulation

| Option | Advantages | Disadvantages | Assessment |
| --- | --- | --- | --- |
| A. Periodic numeric input/output ticks | Simple, fast, deterministic | Less visual activity | **Lean** |
| B. Jobs assigned to individual roaming workers | Visual appeal | Nav, scheduling, failure recovery and AI workload | Defer |
| C. Hybrid: numeric production, cosmetic worker animations | Appears alive with no hard job AI dependency | Must keep visuals honest | Optional later |

Define exact behaviour for missing inputs, full outputs, unloaded chunks, time compression and pausing. No free production from negative elapsed time or repeated load callbacks.

### T-08 - Persistence

| Option | Advantages | Disadvantages | Assessment |
| --- | --- | --- | --- |
| A. Seed + procedural definition version + sparse world edits and entity/POI state | Compact saves, preserves world generation logic | Versioning and regeneration compatibility | **Lean** |
| B. Full world snapshot | Simple semantics, independent of generator changes | Potentially large disk/memory overhead | Useful for debugging |
| C. Chunk snapshot with delta log | Efficient incremental saves at scale | Highest complexity | Defer |

Version save schema explicitly, use atomic temporary-file-and-rename writes, reject malformed data safely, and test save/load equivalence. Never depend on Godot node instance IDs for persistent gameplay identity.

### T-09 - Simulation timing and async work

| Option | Advantages | Disadvantages | Assessment |
| --- | --- | --- | --- |
| A. Fixed simulation tick + render interpolation | Predictable combat/physics, replayable tests | Refactor from variable-step assumptions | **Lean** |
| B. Variable tick for all gameplay | Easy immediate integration | Timing drift and nondeterminism | Not preferred for combat |
| C. Fixed combat + coarse off-screen production tick | Efficient, practical | More scheduling rules | Extend A with this |

Use one authoritative tick order, an accumulator limit, explicit pause semantics and bounded catch-up. Off-screen production can advance by elapsed time mathematically rather than running every game tick.

### T-10 - Live 3D character presentation (decision made for player)

| Option | Advantages | Disadvantages | Assessment |
| --- | --- | --- | --- |
| A. **Live 3D character -> render -> pixel shader -> 2D composition** | Swappable rigged weapons/armour, Mixamo animation reuse, no per-equipment spritesheets | Requires camera/framing, pixel-grid and animation synchronization discipline | **Chosen for player** |
| B. Pre-render 3D animations into spritesheets | Reuses current MultiMesh sprite pipeline | Reintroduces asset baking and equipment/animation combinations | **Rejected for player** |
| C. Convert every entity to live 3D | Uniform art pipeline | Larger rewrite and unnecessary rendering cost for lightweight crowds | **Not planned** |

**Boundary:** This is a **rendering/presentation replacement only**, not a conversion of movement, combat, collision, entity ownership or simulation into 3D. The C++ backend continues to produce player pose/state, facing, position and equipment identifiers through the existing adapter. Godot interprets those values to drive the 3D rig and attachments. Keep normal enemies on their existing spritesheet/MultiMesh path; exceptional bosses may opt into the same 3D path later. Do **not** add a 3D-to-spritesheet baking step.

**Proposed live pipeline:** rigged character + attached equipment -> `Camera3D` / transparent `SubViewport` render -> `SubViewportTexture` displayed in the existing 2D world -> `canvas_item` pixelation shader (pixel grid, optionally palette/colour quantization). A suitable render resolution and camera scale are still visual tuning choices, but the required pixelation happens **after the live 3D render**, not during an offline bake.

## 10. Draft data contracts (illustrative, not final schemas)

These examples express **intent**, not code that should be dropped directly into the present loaders. Assign schema versions, consistent IDs, input validation and migration logic before implementation.

### 10.1 POI definition

```json
{
  "id": "poi.mineshaft.small",
  "blueprint": "mineshaft_small_a",
  "allowed_biomes": ["plains", "highlands"],
  "depth_range": [8, 40],
  "placement": { "must_connect_to_cave": true, "clearance": 3 },
  "spawn_groups": ["mineshaft_common", "mineshaft_elite"],
  "loot_table": "loot.mineshaft",
  "possible_leads": ["boss.buried_warden"]
}
```

### 10.2 Composite quest definition

```json
{
  "id": "quest.hobbit_supplies",
  "objectives": {
    "all_of": [
      { "kind": "deliver_item", "item": "iron_ore", "count": 5 },
      {
        "any_of": [
          { "kind": "kill_entity", "entity_tag": "goober", "count": 5 },
          { "kind": "deliver_item", "item": "goober_token", "count": 1 }
        ]
      }
    ]
  },
  "rewards": [{ "kind": "item", "id": "warden_fortress_key", "count": 1 }]
}
```

### 10.3 Production definition

```json
{
  "id": "production.iron_smelting",
  "period_seconds": 10.0,
  "inputs": [{ "item": "iron_ore", "count": 3 }, { "item": "fuel", "count": 1 }],
  "outputs": [{ "item": "iron_ingot", "count": 1 }],
  "requires": ["structure.furnace"]
}
```

### 10.4 Boss access definition

```json
{
  "id": "boss.buried_warden",
  "access_any_of": [
    { "kind": "use_item", "id": "crafted_warden_summon" },
    { "kind": "unlock_flag", "id": "fortress_gate_open" },
    { "kind": "use_item", "id": "rare_warden_key" }
  ],
  "on_first_defeat": ["unlock.ancient_forge", "event.warden_retaliation_available"]
}
```

**Schema safety:** References must resolve at load time or fail with actionable errors; selectors must validate allowed target types. Never silently convert an invalid content ID into item `0` or an empty world mutation.

## 11. Testing and performance requirements

### 11.1 Pure C++ validation

- Headless tests for terrain edit boundaries, chunk dirtying, collision, entity lifetime/IDs and spatial bucket consistency.
- Unit tests for input buffer timing, attack hit-once semantics, invulnerability, cooldowns, kill events and drop counts.
- Property/fuzz tests for blueprint placement across random seeds, boundaries and overlapping structures.
- Quest tests for `all_of`, `any_of`, item delivery consumption, historical kill counts and one-time rewards.
- Boss-route tests that verify at least one viable access path per required region seed.
- Save/load round-trip tests and version-mismatch handling.
- Tests for deterministic numeric production across large simulated time deltas.

### 11.2 Godot integration verification

- Input modes never trigger a world action while a modal UI consumes it.
- Animation, attack hitboxes and the rendered player model (plus ordinary enemy spritesheets) stay synchronized with C++ state.
- MultiMesh instances display correctly for moving, damaged, dead and removed entities.
- Terrain edits rebuild the right mesh and invalidate local navigation.
- POI assets load, apply correct transforms and persist across world reloads.
- Quest, inventory, boss and event UI show the same authoritative state as C++.

### 11.3 Performance experiments (establish budgets, do not assume results)

| Scenario | Measure | Acceptance principle |
| --- | --- | --- |
| 1,000 and 10,000 lightweight entities | Fixed tick cost, render upload cost, memory | Compare baseline to each major change; diagnose spikes |
| Small active combat encounter | Hit query cost, physics, latency | Smooth, predictable combat at target refresh rate |
| Large edits spanning chunk borders | Dirty chunk count and rebuild latency | No visible stalls or missing mesh borders |
| Frequent kills/pickups | Allocation rate, frame spikes, correctness | No leaks, duplicate drops or stale handles |
| 100+ generated POIs (offline test) | Placement failures, connectivity, overlaps | No unreachable required sites across tested seeds |
| Save/reload in a modified world | Size, load time, structural equivalence | Correct state and acceptable latency |

Record baseline before optimizing. A future 60 FPS target is reasonable for the playable slice, but concrete tick/memory budgets should be chosen after measurement on the intended hardware. Keep separate counters for simple rendered entities versus full AI/physics entities.

---

# Part III - Implementation Plan From the Current Project

## Roadmap policy

- The original detailed Plan's phases 1-6 are the starting point, **not a claim that every acceptance item is finished**.
- These are **implementation milestones**, not one-day promises. Complete one vertical subsystem with tests before expanding it.
- Each phase has `N.1 Native C++`, `N.2 Godot/GDExtension` and `N.3 Completion gate`, mirroring the original PDF layout.
- Each phase ends with a demonstrable player-visible or test-visible outcome.
- Design decisions listed as prerequisites should be recorded in Part IV before beginning that phase.
- **Priority order:** Complete **Step 1** first. Then return to Phase 7 and continue Phase 8 onward. Original phase numbers stay unchanged for easier comparison with `Plan.pdf`.

## Step 1 (Do First): Live 3D Player Rendering with Pixel Shader

**Objective:** Replace **only the player's current spritesheet presentation** with a live-rendered rigged 3D character projected into the 2D world and pixelated **after rendering**. This is a visual-system change, not a rewrite of the authoritative C++ simulation. Keep the existing spritesheet/MultiMesh system for normal entities; consider this pipeline for a few unique bosses only after the player version works.

**Target render pipeline**

```text
Existing C++ backend (unchanged authority)
  -> player position / facing / animation-action state / equipment IDs
  -> Godot player presentation adapter
      -> rigged Character3D + Skeleton3D / AnimationTree
          -> armour meshes + weapon model(s) attached to bones
      -> orthographic Camera3D -> transparent SubViewport (live 3D render)
      -> SubViewportTexture on Sprite2D/TextureRect in the 2D world
      -> canvas_item pixelation shader -> final scene composition

All collision, movement, attack windows, hitboxes and damage remain 2D/C++.
Other entities continue using spritesheets and MultiMesh.
```

### 1.1 Native C++ / GDExtension contract

- [ ] 1.1.1 **Preserve existing backend responsibilities.** Player physics, collision, position, facing, input/action restrictions, attack timings, hitboxes, health and equipment authority stay in C++. Do not move these rules to Godot.
- [ ] 1.1.2 Inventory the **already exposed** player render values (position, facing, current action/animation, timing information, selected/equipped IDs). Reuse them rather than inventing a parallel player controller. Add a minimal read-only field only if the presentation demonstrably requires it.
- [ ] 1.1.3 Define a stable visual-action mapping (`Idle`, `Walk`, `Jump`, `Attack`, etc.) from existing backend states to Godot animation names. If attack progress is already exposed, synchronize against that; otherwise document the smallest adapter change needed. **C++ remains authoritative** for active hit windows, so render playback must not independently create hits.
- [ ] 1.1.4 Keep 2D gameplay coordinates authoritative. Specify one consistent conversion for model scale, facing/mirroring, visual anchor and height above the ground. Rendering offsets must never silently alter collision or hitboxes.

### 1.2 Godot player-rendering implementation

- [ ] 1.2.1 Create a **player-only** 3D visual scene with a humanoid rig (`Skeleton3D`), appropriate mesh nodes and an `AnimationPlayer` / `AnimationTree`. Import a test rig and a small set of Mixamo animations. Confirm skeleton compatibility/retargeting before building more content.
- [ ] 1.2.2 Disable root-motion-driven world movement (prefer in-place clips); the C++ 2D player position determines placement. The 3D rig only displays the action selected by the backend.
- [ ] 1.2.3 Add a fixed orthographic `Camera3D` and transparent `SubViewport` for live character rendering. Display its `SubViewportTexture` in the existing 2D world at the player's authoritative coordinates. Keep camera framing and apparent size stable when animations or equipment change.
- [ ] 1.2.4 Apply a **pixelation shader to the live rendered output** (e.g., a `canvas_item` shader on the 2D presentation surface). Tune block size, nearest-neighbour sampling, filtering and optional palette simplification against the Marching Squares terrain. Do **not** export/bake 3D frames to spritesheets.
- [ ] 1.2.5 Add a weapon attachment at the hand bone (e.g., `BoneAttachment3D`) and equip/swap two weapon models at runtime. Verify correct pivot, orientation, draw order and visual alignment for attack poses.
- [ ] 1.2.6 Add one swappable armour mesh or piece attached/skinned to the same rig. Use compatible skeletons and define clear asset import conventions so future armour doesn't require custom per-animation drawings.
- [ ] 1.2.7 Map the current backend action/facing state to idle, walk, jump and **one attack** animation. Verify visual and collision-space direction agree and the attack appears aligned with the **existing C++ 2D hitbox** at all intended active moments.
- [ ] 1.2.8 Keep normal entities on the current spritesheet/MultiMesh path. No renderer-wide migration, general 3D physics or procedural armour system is required here.

### 1.3 Completion gate

A live 3D player appears correctly inside the existing 2D terrain; the final result is visibly pixelated by a runtime shader; idle/movement/one attack follow the **existing C++ state**; two weapon models and one armour piece can be swapped without generating new spritesheets; 2D collisions and attack hitboxes still align; ordinary spritesheet-rendered entities are unaffected. Establish a simple baseline frame-time comparison, but optimize only if measured overhead is material.

**Scope restriction:** Only one test character, two weapon meshes, one armour piece and essential animation clips. No pre-rendered spritesheet pipeline, no conversion of entity AI/physics to 3D, no wholesale animation-system rewrite.

**Required decision:** T-10 is **chosen** for the player. Remaining art choices (pixel size, palette, lighting and model proportions) should be resolved by testing a live prototype, not by building a general-purpose renderer first.

## Phase 7: Core Integrity, Spatial Index and Simulation Contracts

**Objective:** Close the essential gaps from the original entity phase and create stable foundations for combat, navigation and persistence.

### 7.1 Native C++

- [ ] 7.1.1 Define entity/structure/POI handle conventions, instance ownership and destruction lifecycle.
- [ ] 7.1.2 Implement a chunk-bucket spatial index, with insert, move, remove and query-range operations.
- [ ] 7.1.3 Stop treating spawn-time `entities_on_cell_uID` as authoritative moving occupancy; choose T-02 and migrate queries safely.
- [ ] 7.1.4 Separate position/movement state from immutable type templates where practical; ensure IDs survive container growth.
- [ ] 7.1.5 Establish command and event queue types and a documented safe structural-mutation phase.
- [ ] 7.1.6 Define fixed-step update ownership and test for double-updates, concurrent access and destruction during iteration.
- [ ] 7.1.7 Create headless performance and correctness tests before major optimization.

### 7.2 Godot GDExtension / UI

- [ ] 7.2.1 Preserve present public interfaces through adapters while internal ownership changes.
- [ ] 7.2.2 Render spatial bucket debug bounds and entity IDs on demand.
- [ ] 7.2.3 Confirm MultiMesh rendering is unchanged and removed entities disappear cleanly.
- [ ] 7.2.4 Add tick time, rendered entity count and candidate spatial-query count to a debug overlay.

### 7.3 Completion gate

Moving an entity across chunk boundaries never leaves stale spatial membership; adding/removing entities during simulation is safe; random-range query results match brute-force reference queries; baseline performance is recorded.

**Required decisions:** T-01, T-02, T-09.

## Phase 8: Dropped Items, Actual Inventory and Item Actions

**Objective:** Complete the first end-to-end material acquisition/use loop rather than merely defining items.

### 8.1 Native C++

- [ ] 8.1.1 Implement dropped-item state with type/count/position/velocity, magnetism or proximity pickup and lifetime policy.
- [ ] 8.1.2 Connect inventory slot add/remove/move/split/stack operations to actual player state and authoritative events.
- [ ] 8.1.3 Apply item definition constraints (`max_stack`, hotbar eligibility, durability where applicable).
- [ ] 8.1.4 Implement item-use dispatch for a **small initial set**: mining, placing, consuming, equipping or activating an ability.
- [ ] 8.1.5 Resolve animation-linked item action events exactly once; prevent duplicate resource acquisition or use.
- [ ] 8.1.6 Introduce validated loot tables and `EntityKilled -> DropSpawned -> ItemCollected` event flow.
- [ ] 8.1.7 Define inventory full, stack overflow, despawn and dropped-on-death rules.

### 8.2 Godot GDExtension / UI

- [ ] 8.2.1 Replace the registry browser with an inventory UI displaying **owned stacks** and their counts.
- [ ] 8.2.2 Bind hotbar selection to C++ item slots/actions while retaining remappable custom input actions.
- [ ] 8.2.3 Render dropped items through batching/pooling, pickup feedback and item tooltips.
- [ ] 8.2.4 Introduce usable consumables (e.g., potion), basic crafting UI and meaningful error messages.

### 8.3 Completion gate

Mine an ore -> spawn and collect its drop -> store it -> craft or consume an item -> show consistent inventory and hotbar state. Repeated pickups and full inventories behave correctly.

**Required decisions:** D-05, D-11; select initial subset of `ItemActionType`.

## Phase 9: Animation-Driven Combat Foundation

**Objective:** Replace the original RTS proximity-combat emphasis with a reusable player-and-enemy combat core.

### 9.1 Native C++

- [ ] 9.1.1 Define `AttackDefinition`, `AttackInstance`, hurtbox layers, attack phase windows and target filtering.
- [ ] 9.1.2 Preserve separate movement collision, visuals, hurtboxes and active attack hitboxes.
- [ ] 9.1.3 Connect player frame-specific hitboxes to actual damage queries using spatial broad phase and exact AABB narrow phase.
- [ ] 9.1.4 Implement health, healing, damage types only if needed, knockback, invulnerability frames and death transitions.
- [ ] 9.1.5 Guarantee hit-once-per-target-per-attack semantics and explicit multi-hit overrides.
- [ ] 9.1.6 Implement startup/active/recovery, action buffer, movement input restrictions and sensible cancel policy.
- [ ] 9.1.7 Add one defensive action (dash **or** block), then test it against telegraphed attacks.
- [ ] 9.1.8 Expose combat outcomes as pure events for loot, quest objectives and presentation.

### 9.2 Godot GDExtension / UI

- [ ] 9.2.1 Display damage, hurt, death, attack telegraph and invulnerability feedback.
- [ ] 9.2.2 Provide optional debug overlays for hurtboxes and attack hitboxes; align with actual animation frame/timing.
- [ ] 9.2.3 Add minimal player health UI and combat feedback (hit flashes, sound, camera response, particles as appropriate).
- [ ] 9.2.4 Allow data-driven testing of at least one player attack and one enemy attack.

### 9.3 Completion gate

Player and enemy can damage each other predictably; attacks register only on intended frames and targets; the player can evade a telegraphed attack; death spawns appropriate drops; the encounter **feels responsive** in live play.

**Required decisions:** D-08, D-09, D-10, T-04.

## Phase 10: Enemy AI and Platform-Aware Navigation

**Objective:** Let enemies pursue, patrol and fight in deformable platforming terrain without impossible routes.

### 10.1 Native C++

- [ ] 10.1.1 Define controller intent separately from movement physics (`Idle`, `Patrol`, `Investigate`, `Chase`, `Attack`, `Recover`, `Retreat` as needed).
- [ ] 10.1.2 Build an initial local walkable-surface/jump/fall graph; validate links by actual entity dimensions and movement abilities.
- [ ] 10.1.3 Implement local A* and immediate steering; cache, expire and invalidate routes when relevant terrain chunks change.
- [ ] 10.1.4 Provide perception/aggro via spatial queries and explicit line-of-sight checks where applicable.
- [ ] 10.1.5 Add attack-distance and path-stopping semantics to avoid enemy jitter and overlapping targets.
- [ ] 10.1.6 Optionally extend to hierarchical chunk routes only after profiling and correctness tests.

### 10.2 Godot GDExtension / UI

- [ ] 10.2.1 Render navigation graph, target, path and blocked-edge debug views.
- [ ] 10.2.2 Configure two enemy archetypes via data (e.g., a melee chaser and a ranged/defensive enemy).
- [ ] 10.2.3 Test live terrain edits while enemies are navigating.

### 10.3 Completion gate

Enemies can reach valid platforms, recover from blocked paths, stop at attack range and respond to terrain destruction; no regular full-world pathfinding stalls during combat.

**Required decisions:** T-03, T-09.

## Phase 11: World-State Persistence and Content Loading Contracts

**Objective:** Make procedural content and gameplay progress persistent before adding many quests and landmarks.

### 11.1 Native C++

- [ ] 11.1.1 Define versioned save schema, world-seed persistence, generator version and stable instance-ID allocation.
- [ ] 11.1.2 Save sparse terrain modifications, structures, entities, player inventory/equipment and unlock flags.
- [ ] 11.1.3 Introduce `POIState`, quest-state and event-state records, even if mostly unused in this phase.
- [ ] 11.1.4 Implement safe atomic writes, clean load errors and migration behaviour for schema evolution.
- [ ] 11.1.5 Add deterministic content-loading validation and explicit missing-ID diagnostics.

### 11.2 Godot GDExtension / UI

- [ ] 11.2.1 Add save/load command entry points and a basic loading/error interface.
- [ ] 11.2.2 Rebuild visuals from authoritative reloaded C++ state; do not serialize Godot runtime instance IDs as gameplay state.
- [ ] 11.2.3 Display save location/version in debug mode.

### 11.3 Completion gate

Dig terrain, place a structure, collect an item and defeat an enemy; save, reload and verify identical gameplay state and no duplicate rewards.

**Required decisions:** T-08.

## Phase 12: Authored Procedural POI Placement System

**Objective:** Turn structure stamping into a reliable content-placement pipeline for discoverable mineshafts, homes and fortresses.

### 12.1 Native C++

- [ ] 12.1.1 Define blueprint data (footprint, carve/fill mask, entrance sockets, protected cells, encounter markers, decorations, optional puzzle markers).
- [ ] 12.1.2 Build placement constraints for depth, biome, spacing, clearance, chunk edges and accessibility.
- [ ] 12.1.3 Reserve sites before stamping; preserve player changes and already placed structures.
- [ ] 12.1.4 Link stable POI instance IDs to world state and save records.
- [ ] 12.1.5 Create encounter/loot spawn socket resolver with deterministic seeding.
- [ ] 12.1.6 Add validation for reachable entrances, occupied cells and generated world invariants.

### 12.2 Godot GDExtension / UI

- [ ] 12.2.1 Create one mineshaft, one underground home and one fortress blueprint using a documented authoring format.
- [ ] 12.2.2 Visualize POI bounds, entrances and sockets in debug view.
- [ ] 12.2.3 Add restrained discovery cues: unusual terrain, lights, sound or environmental details.

### 12.3 Completion gate

Across a representative seed batch, all three landmark types generate in valid locations; players can enter/exit them; chunks render correctly; reloading preserves their content and state.

**Required decisions:** D-06, T-05.

## Phase 13: Quest Objectives, Clues and Non-Linear Boss Access

**Objective:** Implement multiple valid ways to obtain one boss's access without writing each quest as a custom gameplay subsystem.

### 13.1 Native C++

- [ ] 13.1.1 Implement an authored objective schema with composable `all_of` and `any_of` nodes.
- [ ] 13.1.2 Support first objective types: deliver item, kill tagged entity, resolve POI, obtain key and trigger interaction.
- [ ] 13.1.3 Subscribe quests to item/combat/world events and persist objective counters.
- [ ] 13.1.4 Prevent repeat rewards and clarify whether inventory or past kills count retroactively.
- [ ] 13.1.5 Model boss access as `any_of` valid routes; verify at least one reliable route exists per world seed.
- [ ] 13.1.6 Implement authored, parameterized dialogue lines with quest state branches; avoid general dialogue generation.

### 13.2 Godot GDExtension / UI

- [ ] 13.2.1 Add NPC interaction, dialogue choices and unobtrusive quest tracking.
- [ ] 13.2.2 Implement the hobbit quest: deliver iron **and** satisfy one of two goober objectives.
- [ ] 13.2.3 Display discovered boss hints without revealing every route or imposing a strict quest order.
- [ ] 13.2.4 Make quest outcome and key acquisition visibly clear.

### 13.3 Completion gate

The player can gain access to the same boss through at least two separate paths. Quest progress survives reload; chosen order does not soft-lock the encounter.

**Required decisions:** D-01, D-07, T-06.

## Phase 14: First Boss, Rewards and Exploration Progression

**Objective:** Prove the central game loop with an authored boss encounter and meaningful post-fight reward.

### 14.1 Native C++

- [ ] 14.1.1 Define boss state machine/phases and telegraphed attack patterns using the common combat system.
- [ ] 14.1.2 Validate arena entrance, summoning conditions, reset conditions and retry/recovery policy.
- [ ] 14.1.3 Assign first-clear reward, repeat-clear loot and unlock flags separately.
- [ ] 14.1.4 Add progression-triggered recipe/equipment availability and optional future retaliation trigger.
- [ ] 14.1.5 Ensure one-time rewards and boss state survive save/load.

### 14.2 Godot GDExtension / UI

- [ ] 14.2.1 Build one polished boss arena, health display, telegraph feedback and intro/outro presentation.
- [ ] 14.2.2 Show crafting/recipe changes after the first victory.
- [ ] 14.2.3 Add repeat-attempt UX that doesn't force redoing an entire dungeon each time.

### 14.3 Completion gate

Reach boss via different routes, survive a demanding but fair fight, claim a meaningful reward, reload and repeat without duplicating first-clear progression.

**Required decisions:** D-01, D-07, D-09.

## Phase 15: Headquarters, NPC Services and Simple Automation

**Objective:** Give the player a satisfying reason to return home, without adding a colony management game.

### 15.1 Native C++

- [ ] 15.1.1 Add NPC service unlock/recruitment state driven by a resolved quest or POI.
- [ ] 15.1.2 Define one production building with validated inputs, outputs, period, storage limit and pause conditions.
- [ ] 15.1.3 Implement mathematically efficient production catch-up for off-screen / inactive visual areas.
- [ ] 15.1.4 Integrate crafting with actual resources and unlocked recipes; prevent duplication across storage/inventory moves.
- [ ] 15.1.5 Save NPC, station, production timer and storage state.

### 15.2 Godot GDExtension / UI

- [ ] 15.2.1 Add a minimal NPC service screen, workshop UI and production status view.
- [ ] 15.2.2 Implement a rescued/recruited NPC arriving at the headquarters.
- [ ] 15.2.3 Add visible furnace operation and a simple completed-output collection interaction.

### 15.3 Completion gate

Complete a POI/quest, recruit an NPC, unlock a crafting service, set up production, leave to explore, return to accumulated output and craft better equipment.

**Required decisions:** D-12, D-13, D-14, T-07.

## Phase 16: One Retaliation Event and Vertical-Slice Integration

**Objective:** Test whether a consequence-driven defense encounter enriches the game rather than interrupting it.

### 16.1 Native C++

- [ ] 16.1.1 Add a one-time milestone event after a specific boss/artifact acquisition.
- [ ] 16.1.2 Define `Pending -> Warned -> Active -> Resolved` event state with explicit triggers and save semantics.
- [ ] 16.1.3 Offer at least two outcomes: defend at home or intercept the attacker at a camp.
- [ ] 16.1.4 Define temporary, recoverable consequences when the event is ignored or lost.
- [ ] 16.1.5 Reward success with a meaningful item, service or clue; avoid requiring siege participation for the next boss.

### 16.2 Godot GDExtension / UI

- [ ] 16.2.1 Present warnings and map clues for attacker movement/encampment.
- [ ] 16.2.2 Create one small siege/interception enemy group; reuse existing combat and pathing.
- [ ] 16.2.3 Add simple optional barricade or trap building, not a fully autonomous defense AI.
- [ ] 16.2.4 Test the complete 30-60 minute vertical slice with fresh players and record confusion/friction.

### 16.3 Completion gate

The main exploration -> multiple boss routes -> reward -> base improvement loop stands on its own. The retaliation event is understandable, optional in approach and not a recurring nuisance. Remove or redesign it if playtesting shows it weakens the experience.

**Required decisions:** D-15, D-16, D-17.

## Phase 17 (post-slice): Quality, Content and Scale

**Objective:** Expand only the systems proven to be enjoyable.

### 17.1 Native C++

- [ ] Profile 1,000/10,000 entity workloads and reduce unnecessary per-entity copies or full-world scans where measurements support it.
- [ ] Add hierarchical pathfinding if world-scale route requests actually need it.
- [ ] Expand POI grammar, enemy types, quests, loot tables and regional encounter budgets.
- [ ] Add versioned content migration for saved worlds and robust automated seed regression suites.

### 17.2 Godot GDExtension / UI

- [ ] Improve combat sound, particles, camera, enemy readability and animation quality.
- [ ] Add discoverable crafting recipes, map hints, accessibility options and coherent visual identity.
- [ ] Build save-slot, settings, pause, menu and end-to-end new-player onboarding flows.
- [ ] Test multiple resolutions, input methods and graphics performance profiles.

### 17.3 Completion gate

Content and performance increase without breaking the vertical slice, save files or combat responsiveness. Do not expand before measuring retention of the core experience through playtests.

---

# Part IV - Decisions, Dependencies and Scope Control

## 12. Decision register - fill this in before implementation

Use `Chosen` for the selected letter and add rationale/date in your own words. The default is **undecided**, even where a lean is marked above.

| ID | Choice | Current lean | Chosen / notes |
| --- | --- | --- | --- |
| D-01 | Progression gating | A, soft tiers | TBD |
| D-02 | Expedition pressure | A, natural risk | TBD |
| D-03 | Return travel | A, shortcuts | TBD |
| D-04 | Death penalty | No lean yet | TBD |
| D-05 | Inventory friction | Limited slots to test | TBD |
| D-06 | Discovery guidance | A + optional rumours | TBD |
| D-07 | Boss summon / key policy | C for slice | TBD |
| D-08 | Defensive combat verb | A, dash | TBD |
| D-09 | Combat scope | A, small polished set | TBD |
| D-10 | Terrain in combat | A, traversal/mining | TBD |
| D-11 | Recipe discovery | A basic, B rare | TBD |
| D-12 | Service unlocking | C, NPC + station selectively | TBD |
| D-13 | Production kinds | A first, then B | TBD |
| D-14 | Storage model | A, shared base stockpile | TBD |
| D-15 | Retaliation scale | A, one authored event | TBD |
| D-16 | Siege failure consequence | A, temporary service disruption | TBD |
| D-17 | Siege optionality | A, defend/intercept | TBD |
| T-01 | Entity storage/index | A, map + chunk buckets | TBD |
| T-02 | Cell occupancy | B, separate chunk index | TBD |
| T-03 | Navigation graph | B then C if needed | TBD |
| T-04 | Attack data model | A, JSON + C++ executor | TBD |
| T-05 | POI blueprint assembly | A first, B later | TBD |
| T-06 | Quest representation | A, authored predicates | TBD |
| T-07 | Production model | A, numeric ticks | TBD |
| T-08 | Persistence model | A, seed + diffs | TBD |
| T-09 | Simulation timing | A + coarse off-screen ticks | TBD |
| T-10 | Player visuals: live 3D render -> pixel shader -> 2D | A | **Chosen: A (player; optional unique bosses later)** |

## 13. Dependency and milestone gates

| Milestone | Depends on | Proves |
| --- | --- | --- |
| M0: Live 3D player presentation | Step 1 | 3D equipment and Mixamo animations appear as pixelated 2D visuals without changing C++ authority or normal-entity MultiMesh |
| M1: Entity/query integrity | Phase 7 | Safe shared substrate for damage and AI |
| M2: Complete loot loop | M1, Phase 8 | Resources are actually obtainable and usable |
| M3: Combat fun prototype | M1, Phases 9-10 | One enjoyable fight with readable movement and attacks |
| M4: Persistent authored world | Phases 11-12 | Interesting structures exist and survive reload |
| M5: Non-linear boss | M2-M4, Phases 13-14 | Player reaches boss through different approaches |
| M6: Adventure headquarters | M2, M4-M5, Phase 15 | Returning home feels worthwhile |
| M7: Retaliation experiment | M3-M6, Phase 16 | Events complement rather than interrupt the main loop |

**Critical path:** **Step 1 / M0 -> M1 -> M2/M3 -> M4 -> M5 -> M6.** M7 is optional for validating the core game. This ordering permits experimentation with combat before writing a large procedural POI library.

## 14. Vertical-slice acceptance walkthrough

An unfamiliar player should be able to do the following without a developer explaining hidden commands:

1. Spawn with a **live 3D-rendered, shader-pixelated player** whose animations and swappable equipment reflect C++ state, plus basic movement, weapon, tools and understandable health/consumable UI.
2. Explore a cave and discover an abandoned mineshaft.
3. Fight a recognizable enemy, collect a meaningful item and see a clue about its use.
4. Find a friendly underground inhabitant and accept a quest with alternative objective completion.
5. Enter a fortress and solve one readable environmental puzzle.
6. Obtain access to a boss through **either** the quest/key route **or** an alternative route.
7. Fight and defeat the boss after one or more attempts, with predictable recovery/retry mechanics.
8. Return, recruit one NPC or unlock one service, and craft one improved item.
9. Run one simple production building while exploring elsewhere; return to collect the output.
10. Optionally experience a signposted retaliation and choose defense or interception.
11. Save, quit, reload and find the changed world, inventory, quest and progression states intact.

### Success questions for testing

- Did the player enjoy going out again after returning home?
- Did they find a meaningful landmark without following a strict quest marker?
- Did combat failures feel fair, readable and learnable?
- Did they recognize that more than one route could unlock the boss?
- Did returning to base feel rewarding, or merely obligatory inventory management?
- Was retaliation exciting, or an unwanted interruption? If unwanted, remove it rather than elaborating it.

## 15. Deferred backlog / expansion candidates

- Multiple factions with territorial behaviour and autonomous diplomacy.
- Persistent armies and broad RTS command interfaces from the original roadmap.
- Fully procedural villages and citizens with simulated schedules.
- Player-commanded companions (including a future Pip companion prototype) with contextual memory.
- Deep production chains, labour networks and physical item logistics.
- True hierarchical platformer pathfinding and large-scale route caching everywhere.
- Rich scripted puzzles and procedural quest grammars beyond a bounded template library.
- More elaborate terrain-reactive combat, fluid interactions and cascading structural collapses.
- Online multiplayer, competitive balancing and networking architecture.

## 16. Change log

| Date | Version | Change |
| --- | --- | --- |
| 2026-11-17 | 0.1 | Initial game-design concept, architectural alternatives and roadmap from repository baseline |
| 2026-11-18 | 0.2 | Added Step 1: live 3D player rendering with post-render pixel shader, rigged equipment and Mixamo animations; preserved C++ simulation and sprite/MultiMesh enemies |

---

## Final design checkpoint

Before expanding content, answer these in order:

1. **Is one ordinary fight fun?** If not, polish combat before adding bosses.
2. **Is one dungeon worth exploring?** If not, improve structure layout, clues, enemy mix or rewards before generating dozens.
3. **Are two routes to the same boss genuinely interesting?** If not, revisit the access/reward design.
4. **Is the headquarters a pleasant five-minute stop?** If not, simplify crafting, storage and travel.
5. **Do rare retaliations improve the expedition fantasy?** If not, they remain optional or are removed.

**Working north star:** *The player is an adventurer first. Structures create discoveries, bosses create direction, and the home base makes the next expedition possible.*
