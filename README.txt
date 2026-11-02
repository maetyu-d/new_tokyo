YORU / TOKYO 2091 — 1.26 / EASIER BUILDING

Build / Tab opens one catalogue for every building, road, utility, supported
route, structural tool and reusable plan. Press /, type a name such as homes,
road, lift or bridge, then Enter. Up and Down select another search result.
Category tabs and scrolling expose the full catalogue without adding permanent
controls to the city view.

Selecting a tool pauses the city, raises the zoom to a useful building scale
and cuts away floors above the selected level. [ and ] change that level.
Cancel / Esc finishes construction and restores the previous stack view.

Click or drag to place buildings. A continuous drag fills intervening tiles;
the brush stays selected for the next stroke. All buildings in that drag are
one undoable edit. The city forecasts their services once on release instead
of after every tile. U, Cmd/Ctrl-Z, or the Undo button reverses the whole stroke.

Point at an existing building and press I, or select it and choose Build
another. This copies its type, chooses its actual floor and centres the build
view there. Copies are new buildings with the normal cost and starting stock.

The placement ghost uses the same rules as the actual placement. The compact
build dock explains occupied space, missing decks, catwalk restrictions, river,
connector clearance and insufficient funds. Unsupported decks receive a bearing
warning. Tool / Undo / Cancel controls appear only while building.

Search lifts, skybridges, stairs, ramps, moving walks or supported streets to
open the continuous route composer with bearing cores enabled. Choose landings
and floors, review the route and its supports, then Enter to build it as one
undoable edit. The N palette still offers individual construction parts.

The full 154 x 70 demo and all saved cities remain in their existing profiles.
Both machine and top-down views share the new construction controls.

Earlier release notes follow.

YORU / TOKYO 2091 — 1.25 / TWICE THE WIDTH AND DEPTH

The new expanded demo measures 154 x 70 ground tiles, twice the original
77 x 35 footprint on each axis and four times the ground area. It contains
2,592 buildings, 60,916 initial residents, 14,931 occupied cells across all
strata, and 1,546 transfers. The skyline retains occupied floors at L0–L12.

Four complete inhabited quarters retain the original building density. Streets
meet across both seams; ground rail gains north/south spines joining the two
east/west lines and their elevated approaches. Eighteen additional L6 skyway
spans use real supported landings and piers, with no span longer than twelve
tiles. Local utility circuits remain separate, with an additional seam-side
feed so the original homes retain service.

Normal launch and Start Yoru Megacity.command now resume the expanded demo in
its separate megacity-2x save. Your previous city remains in megacity-dense;
Start Original City.command or --demo opens it. --demo-2x explicitly selects
the expanded city, and --starter retains the existing starter. New City offers
2 x 2 demo, Original demo, Compact starter and Blank sandbox.

F fits the entire larger city. Wheel zoom reaches closer quarters or a wider
overview; both the machine and top-down views use the same expanded city.
Recent frame-rate improvements and continuous optical pan/zoom remain.

Validation covers exact doubled bounds, four-quarter density, street and rail
connectivity, supported skyway geometry, persistence, full utility and food
coverage through three running ticks, all launch profiles, the actual New City
button and full-city views. Existing saved cities remain unchanged.

Earlier release notes follow.

YORU / TOKYO 2091 — 1.24 / CONTINUOUS OPTICAL BACKGROUND

Panning and zooming preserve the optical feedback instead of clearing it. The
previous image is reprojected to the same world positions in the new camera
view, including pointer-anchored zoom. New space uses current city light rather
than stretching old edge pixels.

The bounded set of 32 optical sources is selected from the city independently
of the camera and viewport. Buildings crossing the edge of the screen no longer
reshuffle the whole pattern. Inactive components retain their source identity
with zero intensity. The background keeps its normal motion and feedback.

The large city, simulation, saves, render quality and 1.23 performance work are
retained. There is one affine mapping inside the existing feedback pass, with
no additional full-screen pass. Native UI and both city views remain available.

Validation includes a GPU patch-retention test for pan, zoom in/out, viewport
rescaling and newly exposed space; 709 projection checks; 1,737,600 rendering
checks; native cache, optical controls, flow and full-city camera checks. All
actual save files remain byte-identical. --optics-navigation-check runs the GPU
diagnostic. Detailed evidence: build/optics24-validation.json.

Earlier release notes follow.

YORU / TOKYO 2091 — 1.23 / RETAIN THE CITY MACHINE

Static shafts, stairs, ramps, trusses and landings now stay in GPU memory while
the camera moves. Cabins, escalator treads and moving walks continue to animate.
Unchanged skins, city layers and network colours reuse their prepared assets
between completed forecasts. Network vertices read shared curve corners rather
than each reconstructing the same curve and its normals.

On the saved 18,640-person, 648-building, L0–L12 city, alternating native runs:
  Playing:                    44.7 to 68.9 FPS (+54%).
  Animated overview/crown/pan: 44.6 to 72.5 FPS (+63%).
Same 1380x790 logical / 2760x1580 physical window, Balanced 2070x1185 scene,
original 4x MSAA request and optical effects. Two alternating runs per release.
All playing runs commit three ticks and produce identical saved cities.

Visible windows now target 60 FPS even when unfocused; the former unfocused
limit was 15 FPS. Minimized windows target 5 FPS. A separate run with normal
frame pacing at 1380x810 logical / 2760x1620 physical averaged 56.2 FPS;
its median frame was 16.9 ms and its 95th percentile 22.0 ms. These are
measurements on this shared host, including forecast publication, rather than a
locked frame-rate guarantee. RENDER-PERFORMANCE.txt records limits and evidence.

Curve detail now uses 4/6/8/10/14 segments with the existing 0.35-physical-pixel
error bound. The crown and moving-camera checks submit about 18% fewer network
vertices; the fitted overview retains six segments. The native UI, optical
shaders, feedback formats, large saved city, simulation and game depth remain.

Earlier release notes follow.

YORU / TOKYO 2091 — 1.22 / FASTER DRAWING

The renderer now streams transient GPU batches without overwriting data still
being read by earlier passes. It also skips uploads for shader inputs that are
unused. City geometry, detail settings, optics, native UI and simulation retain
their existing behaviour.

On the saved 18,640-person, L0–L12 city, alternating uncapped runs at 1380x790
logical / 2760x1580 physical, Balanced quality and the existing effects measured:
  Playing, three committed ticks: 12.1 to 40.4 FPS (+233%).
  Animated overview/crown/pan:     17.2 to 43.3 FPS (+152%).
These are mean per-run throughput measurements on this machine, with three playing runs
per upload mode and two animated-view runs per release. The playing set includes
a same-binary legacy/streaming comparison. Results vary with hardware, view and competing work.
Normal play retains its existing foreground and background frame limits.

All six original raw scene hashes and six full-optics comparison hashes match
between the streaming and legacy upload paths. Every playing run produces the
same saved city after three ticks. No lower quality preset is substituted.
Controls, save profiles and the normal saved large-city launch are retained.

Detailed measurements and reproduction are in RENDER-PERFORMANCE.txt.

PREVIOUS TRANSPORT AND GAME GUIDE

YORU / TOKYO 2091 — 1.21 / THE VERTICAL DAY

LIFT BANKS
Inspect a lift landing and choose Lift bank service, or M / Vertical / Service.
A plan controls the existing shaft of that lift type at that address. Local
service stops at its physical landings. Express service uses your selected floors;
it never opens a door where no connected landing exists. Build a sky lobby with
real decks and walkways to change between local and express shafts. Boarding each
service adds a transfer cost, so direct express and interchange layouts differ.

Every car has a finite seat-minute budget per ten-minute interval, shared across
all rides in its shaft. Adding cars requires upgrading every physical flight.
Freight reservations divide car time between passengers and portered goods.
Passenger banks still cannot carry alloy or scrap; those require a goods shaft.
A goods shaft never acquires passenger access from a reservation setting.

Set a maintenance start hour and duration to take one car out of service each day.
A one-car bank suspends during its window; other transport remains available.
All plan edits stay in a draft until Apply service; one undo reverses an edit.
Removing a plan keeps its physical shaft and restores ordinary transfer operation.

DAILY DEMAND
City health and economy, or the lift plan inspector, offers Daily demand. Existing
saves retain steady demand until you enable it. One tick advances ten minutes;
144 ticks make a day. Clock and demand phase appear in the existing city controls,
vertical kernel and dependency inspectors.

05–07 delivery window; 07–10 morning commute; 10–16 daytime services; 16–20 evening
commute; 20–05 late/night activity. Commuter route pressure increases at rush hours;
evening trips run from employers toward homes. Freight windows alter finite dock
throughput; care and leisure demand have their own daily rhythms. Employment
coverage is normalized separately from interval trip pressure. Inventories, food
consumption, utility supply and actual route capacities remain physically counted.

QUEUES AND TRACES
Unserved requests at a binding lift become aggregate landing queues for the
current interval. Queued goods stay in their source inventories and residents
stay in their homes; the next forecast rebuilds requests. These are pending
interval trips, rather than individually simulated people or persistent bookings.
People/Freight layers and dependency traces mark queues at real stopping landings.
The service inspector reports car time, pending trips/goods and backlog clearance.
Building and district dependency panels expose related queues and open their bank.
Delivered routes retain every physical motor dependency and their real levels.

Simulation uses aggregated flows and existing background ticks. No transport
route search runs per drawing frame. The default minimal HUD is retained. New
transport metadata uses save schema eight at the existing save path; schemas one
through seven still load. Large-city launch and independent save profiles remain.

PREVIOUS DISTRICT AND GAME GUIDE

YORU / TOKYO 2091 — 1.20 / DISTRICTS WITHIN THE MACHINE
A city without an end. Native C++17 / macOS arm64.

J / DISTRICT VOLUMES
Create a named neighbourhood from one or more rectangular volumes. Choose its
low and high floors, then Draw another volume and click two footprint corners
in either view. A district can stack directly above another on separate floors,
or unite several detached pieces. Different districts cannot occupy the same
address. Up to 64 districts and 16 volumes per district are supported.

Type or paste the name; Cmd/Ctrl-A replaces it. Draw, redraw and remove work on
a draft. Apply district activates the entire edit as one undoable action. U /
Cmd-Ctrl-Z reverses it before another edit or tick. Delete removes the district
metadata while keeping buildings, infrastructure and inventory in place.

OPERATIONS
Set an operating cap in yen per tick, or leave it Unlimited. The panel shows
actual required and funded running costs. A shortage throttles local operations,
reduces link capacity and increases wear; passive links retain some capacity.
The budget funds running costs rather than placing money in a separate treasury.

Power, water, freight and people each have Inherit / Critical / Normal / Low
priorities. These change dispatch order under competition for real supply and
network capacity. The district operating mode also sets its existing production
and conservation policy. Unassigned components retain geographic ward policies.

Reserve targets hold actual material in local warehouses. An outside ordinary
request draws only surplus; local consumers and critical freight requests can
use protected stock. Replenishment uses remaining real route capacity after
primary consumers. A target creates neither inventory nor warehouse space, and
cannot move freight across a passenger-only connection. Waste still goes to
recyclers. Shortfalls are visible in the trace panel.

EXCHANGES AND SHARED LINKS
Open Exchanges or Shared links, then Trace district in city. The panel shows
actual incoming/outgoing power, water, commodities and people, exchange partners,
physical warehouse reserves and exact routes. Gate trade is attributed at the
real gateway. Shared links identify lifts, bridges and trunks that serve multiple
areas, including local infrastructure used by outside districts. Locate a link
or probe a transfer failure through the existing resilience inspector.

Filter a channel, include internal routes, fit the volume or routes, and select
any paged route to isolate it. T switches between skyline and diagram; [ / ]
changes the diagram floor. Vertical paths retain their real landing levels.
The overlay shows up to 96 routes together, with every path available by row.
Analysis runs on one background snapshot and refreshes with the city forecast.
No extra HUD strip is required: J and Menu open district controls when needed.

SAVES AND LARGE CITY
Normal launch still resumes the saved large L0–L12 city and fits its skyline.
Cities without custom districts retain schema six. Creating district metadata
uses schema seven at the same existing save path. Versions one through seven
load in this release. District inspection and cancelled drafts do not change
the city. Existing route composer R and building trace G remain available.

PREVIOUS ROUTE COMPOSER AND GAME GUIDE

YORU / TOKYO 2091 — 1.19 / ROUTES THROUGH THE MACHINE
A city without an end. Native C++17 / macOS arm64.

R / CONTINUOUS ROUTE
Click a start and successive waypoints. [ and ] change the active level while the
draft stays open. Choose Next to switch each new leg between a walkway, street,
maglev, bridge, moving walk, pedestrian ramp, stairs, passenger/express lift,
road ramp, goods lift or escalator. Surface legs bend orthogonally; the Bend
control changes their order. Transfer spans use the existing physical limits.

Blue cells show new decks; gold members show every proposed core. Add cores
creates explicit support where needed. Existing support mode adds no columns.
New surface and end-landing decks use the chosen material; core floors keep their
standard construction and existing decks retain theirs. Blocked geometry,
unsupported landings and insufficient credits prevent the whole project. Bearing
stress is reported separately; an overloaded structure needs reinforcement.
The cost and new motor demand appear before construction. Motors still need
real electricity; the composer does not wire them invisibly.

Clearance and support checks run on one background snapshot while you draw.
A yellow preview is still being checked. Enter queues the fixed draft for
construction once its exact check completes; changed or cancelled drafts discard
older results. No draft or preview job can save a city.

Enter or Build places all fixed legs together. The pointer's ghost leg is never
committed. One U / Cmd-Ctrl-Z undoes the entire project before another edit/tick.
Backspace removes a draft waypoint. Q / Escape discards the draft. The city stays
paused while composing. R is also in N / Walkways and transfers.

G / TRACE THE MACHINE
Select any building and press G, or use Trace dependencies near the top of its
inspector. Follow its actual power, water, goods and people routes in the city.
The panel filters channels, lists exact quantities and levels, fits the traced
routes, isolates an individual path and follows its source. In diagram mode,
level portals show where a path leaves the current floor.

Direct dependencies include inputs and outputs. Supply chain also follows
providers' inputs, with a three-step / 128-component bound. The panel identifies
that bound when reached. The city overlay draws up to 96 paths at once; selecting
a row isolates any listed path. No delivery is invented when a service is absent.
Pink links mark shared load at or above 85% or actual pipe loss. Locate a constraint
or select a route and probe its transfer to forecast affected buildings and losses.

Traces refresh after forecasts, edits, undo and loads. Utility provenance is
aggregated on the exact dispatch paths; transfer motor power is recorded too.
Inspection performs no simulation or route search. The renderer's existing
optimisations remain; trace overlays are drawn separately from the paused city.
Save schema six and the default saved large-city launch remain unchanged.

PREVIOUS LAUNCH FIX AND GAME GUIDE

YORU / TOKYO 2091 — 1.18.1 / RESUME THE LARGE CITY
A city without an end. Native C++17 / macOS arm64.

Double-click Yoru Tokyo.app or Start Yoru Tokyo.command to resume the saved dense
vertical city and fit its full skyline. Start Yoru Diagram.command opens that same
city in the top-down view. The independent megacity-dense/city-v6.json is retained;
opening the app does not regenerate it. A missing dense save creates the large
preset once. Start Yoru Megacity.command remains available.

--starter opens the separate compact city at city-v6.json. --fresh-city and --blank
retain their existing starter/sandbox behaviour; --demo --fresh-city explicitly
regenerates the dense demo. YORU_DATA_DIR and diagnostic runs keep their root save
behaviour; add --demo to use an isolated dense profile. Diagnostics without an
explicit data directory use temporary storage. Existing saves stay at their paths.

PREVIOUS FLOW OPTIMISATION AND GAME GUIDE

YORU / TOKYO 2091 — 1.18 / LIGHTER FLOW UPDATES
A city without an end. Native C++17 / macOS arm64.

LIGHTER FLOW UPDATES / NEW IN 1.18
Changing service levels now refreshes only affected ranges of the existing flow
colour buffers. Paths, curve meshes, layer indices and animation tables stay in
place while the network's layout and activation remain the same. Identical
rendered colour bytes cause no GPU upload. RGB pigments are reused; service
changes update the original alpha values without extra HSV conversions.

Changing layout, curve detail or crossing the existing flow-activation threshold
still triggers a full mesh rebuild. No route, service value, capacity, city
rule, save format or drawing precision changes. Existing simulation-thread and
paused-view optimisations remain. F3 shows FPS; F4 cycles rendering quality.
See RENDER-PERFORMANCE.txt for measured update costs, frame times and limits.

PREVIOUS FRAME-PACING WORK AND GAME GUIDE

YORU / TOKYO 2091 — 1.17 / SMOOTHER CITY MACHINE
A city without an end. Native C++17 / macOS arm64.

SMOOTHER FRAMES / NEW IN 1.17
Automatic city ticks now run on one independent simulation worker. Drawing,
navigation, weather and optical feedback continue while the existing calculation
runs. Only a complete result can enter the live city. Edits and different loads reject
stale results; paused, modal and painting frames do not accept a late result.
The worker cannot save a file.

A stopped city reuses its geometry image, including the top-down process floor.
Camera movement, changing animation, construction and selected animated journeys
use the live drawing path. Rain stays live outside the geometry cache; native
optical filaments and the native UI remain. Network chunks share their shader,
texture and projection bindings, and flow lookup uploads rotate across three
small textures to avoid overwriting the preceding GPU frame.

F3 displays frame rate. F4 cycles Native / Balanced / Fast. Balanced remains the
default. All construction, vertical circulation, city rules and schema-6 saves
are retained. See RENDER-PERFORMANCE.txt for actual running-city frame timings
and the distinction between rendering speed and simulation calculation speed.

PREVIOUS ROUTE WORK AND GAME GUIDE

YORU / TOKYO 2091 — 1.16 / LIGHTER ROUTE SEARCHES
A city without an end. Native C++17 / macOS arm64.

LIGHTER ROUTE SEARCHES / NEW IN 1.16
Route queues now pack travel cost and graph-node order into one integer key.
Heap comparisons use that key directly, preserving the original cost-first,
node-second ordering. Utility dispatch, freight, commutes, care/leisure journeys,
water-head reach and workshop distances all use the lighter queue. The queue
reuses its storage within each forecast.

A search stops when its cheapest pending route exceeds the existing journey
budget. Longer pending routes cannot become eligible, so they need no further
processing. Trip limits, physical capacities, source priorities and path choices
are unchanged. Rendering, abstract forms, all construction tools and saves retain
their existing behaviour.

A calculation-only diagnostic now loads a city once, warms the simulation, then
measures repeated immutable forecasts. It reports CPU and wall time separately:
  simulation_benchmark /path/to/copied/city-v6.json 5
This excludes save loading, rendering and JSON export from measured samples.
See RENDER-PERFORMANCE.txt for measured limits and reproduction details.

PREVIOUS CONNECTIVITY AND GAME GUIDE

YORU / TOKYO 2091 — 1.15 / FASTER CITY CONNECTIVITY
A city without an end. Native C++17 / macOS arm64.

FASTER CONNECTIVITY / NEW IN 1.15
Ground-exit analysis prepares its destination lookup once for the forecast.
Each home's two-route search reuses compact flow, queue and parent buffers instead
of creating tree entries for visited edges. Residual cancellation still finds
independent exits correctly, including routes behind shared approaches. When no
safe ground destinations exist, the result is immediately zero.

Building connection points also use a direct lookup cache. Their at-most-four
nodes live inside each cache entry, removing small vector allocations. Graph IDs,
provider order and cost tie-breaks retain their existing order. Every forecast
recreates these caches, so edits, outages, undo and loads see fresh connectivity.
The same routes, finite capacities, deliveries, vertical access and blackout
results remain. All rendering, flow strands, abstract forms and UI are retained.

PREVIOUS SHARED FLOWS AND GAME GUIDE

YORU / TOKYO 2091 — 1.14 / SHARED FLOW GEOMETRY
A city without an end. Native C++17 / macOS arm64.

LIGHTER FLOWS / NEW IN 1.14
Power, water and rail filaments now reuse a small table of animated bends and
edge directions. Every link with the same length, strand and phase shares those
calculations. Its vertices fetch the result instead of rebuilding the same curve
at every corner. Paused flows reuse the table without recalculating or uploading.
The four pigment strands, original widths, phases, moving carriers and contacts
remain. The combined scene keeps the same geometry and drawing order.

Service views submit only their own layer's triangle indices. Other layers are
skipped before vertex processing; the complete view still uses the original draw
list. Floors, cutaway, camera culling, quality modes and live service state retain
their existing behaviour. Route searches also reuse their frontier and node buffers. Search epochs mark
only the nodes touched by the current route instead of clearing every node before
every request. Edge capacity and current loads are still read during each search;
provider order, tie-breaks, route costs and all simulation rules are preserved.
People and freight overlays also read the stored paths directly, avoiding a fresh
route-vector allocation and copy for every journey or shipment on every frame.
The city layout, abstract forms, diagram and save format remain unchanged.

PREVIOUS ABSTRACT FORMS AND GAME GUIDE

YORU / TOKYO 2091 — 1.13 / ABSTRACT BUILDING FORMS
A city without an end. Native C++17 / macOS arm64.

ABSTRACT FORMS / NEW IN 1.13
Buildings now use seven simple families: solid blocks, slender blades, paired
masses, eight-sided drums, raised plates, wedges and open portals. Unbroken flat
faces and restrained edges replace windows, signs, roof equipment, chimneys,
trees, decorative bands and repeated rings. Building type retains its function
colour, with one small operating point and the existing service warning.
Construction ghosts, selection, upgrades and disabled appearances use the same
forms. Stacked floors, open frames and the spaces between components remain
legible across all thirteen strata. The live service networks and Afterimage
optics continue to express the city as a working machine.

Identical forms now share static artwork across addresses; it no longer needs
periodic animation repainting. Operating points animate separately. The minimal
interface, twenty-eight components, full simulation, vertical construction and
Aftertaste-style diagram remain available. Existing cities and saves retain
their paths, geometry and rules.

PREVIOUS MINIMAL INTERFACE AND GAME GUIDE

YORU / TOKYO 2091 — 1.12 / THE CITY FIRST
A city without an end. Native C++17 / macOS arm64.

MINIMAL INTERFACE / NEW IN 1.12
The city now has one quiet, forty-pixel control bar. The default view uses ten
visible buttons instead of thirty-five. The large title, repeated view controls,
permanent resource counters, fifteen-button overlay strip and shortcut footer
are gone. F fits the taller, less obstructed canvas. The simulation is unchanged.

The people count opens city health, power/water/food, wellbeing, care, employment,
transit, budget, tax, economy mode and the navigable district map. Service deficits
and structural problems surface as a compact issue count even when it is closed.
Layers / L opens all fifteen engineering and population layers; V still cycles.
The active layer's name stays in the bar. Click L0–L12 for the level picker,
Full stack / Cut above and vertical tools. [ and ] still move between strata.

Build / Tab opens the searchable catalogue of all twenty-eight components and
route tools. Choosing a component or construction tool clears the library; a
small temporary chip names the exact tool, material, floor and pending landing
or corner, with a Cancel action. N still opens every structural material,
walkway, transfer and reusable frame. B still opens the physical building plans.

Click any installed component or infrastructure tile to open its inspector alone.
Build and inspection replace each other. The inspector uses the full panel height
for service state, stocks, workload, priority, upgrades, repairs and transfers.
City-wide signals and the map live in the city summary instead of taking up the
component's workspace. Selection is kept out from behind the inspector.

Menu contains checkpoints, restore, new city, fit, step, plans, the guide,
optics, rain, render quality and FPS. M still opens the complete nine-tab machine
kernel: process, inventories, dispatch, districts, history, vertical, people,
engineering and resilience. T switches machine / diagram. Space runs or pauses;
. steps one tick. The speed button cycles 1x / 3x / 8x.

F2 or Menu / Hide UI enters a chrome-free view in either projection. Construction
is cancelled before hiding so no invisible tool can modify the city. F2 or Escape
restores the interface; selecting a component also brings its details back.
Escape otherwise backs out of the current panel or tool. On Macs with media
function keys, hold Fn when using F2/F3/F4. Autosave, checkpoints, dated new-city
backups, all model rules and existing cities remain unchanged.

PREVIOUS FRAME-RATE, DENSITY AND CONSTRUCTION GUIDE

YORU / TOKYO 2091 — 1.11 / FRAME-RATE CONTROLS AND CACHED SCENES
A city without an end. Native C++17 / macOS arm64.

FASTER RENDERING / NEW IN 1.11
F4 cycles Balanced, Fast and Native rendering for this session. Balanced is the
new default: the city uses 75% of native width and height (about 44% fewer scene
pixels). Fast uses 50% of each dimension (75% fewer pixels). Native restores the
full-resolution city. These modes trade city-image sharpness for GPU cost; no
buildings, structures, roads, skyways or optical effects are removed. The interface
and the top-down diagram always render at native display resolution.
F3 displays FPS, frame time and the current mode. On Macs with media function
keys, hold Fn with F3/F4. An active window retains the
60 FPS cap; unfocused windows use 15 FPS to reduce background CPU/GPU load.

Native rendering is also lighter. Network quads share their triangle vertices,
reducing submitted vertices by one third. A precomputed curve basis and seventeen
shared phase pairs replace the network vertex shader's repeated trigonometry.
All four animated strands, carriers, contacts and moving signals remain.

Ground and static structural floors are cached at the chosen scene resolution.
Materials, neighbouring junctions, support, edits, cutaway, level, resize and camera
state invalidate the right layers. Moving traffic, lift cabins, escalators,
walking signals, building motion, inspection and construction stay live. Camera
motion uses direct drawing; the caches settle after it stops, avoiding repeated
full-screen cache rebuilds while panning.

The optical neighbourhood field is cached in full-precision float textures while
the view and its building inputs are stable. Time-dependent feedback and light
still run every frame. A separate direct shader keeps the original calculation
while the camera moves. Uniform locations are cached; unused depth attachments
and their clears have been removed from the 2D render targets.

The city model, schema-six saves and dense demo are unchanged. For controlled
performance comparisons, use --render-quality native or balanced explicitly:
  YoruTokyo --fit --render-quality native --render-benchmark result.json
The benchmark exports native framebuffer and actual scene dimensions separately.
See RENDER-PERFORMANCE.txt and VALIDATION.txt for checks and measured limits.

PREVIOUS RENDERING, DENSITY AND CONSTRUCTION GUIDE

YORU / TOKYO 2091 — 1.10 / FASTER CITY RENDERING
A city without an end. Native C++17 / macOS arm64.

RENDERING / NEW IN 1.10
Animated power, water and rail filaments now run on cached GPU meshes. Meshes are
split into local world chunks, culled against the camera, and rebuilt only when
installed geometry or its visual service state changes. All four pigment strands,
carrier lines, live dots and building contacts remain. Distant curves use fewer
segments with a subpixel error bound; close views retain the previous detail.

Buildings, floor materials, piers and street junctions share a bounded, native
resolution sprite atlas. New edits, material conversions, support failures,
service warnings, upgrades, undo and loads select the correct appearance.
Building motion, traffic, lifts, smoke, selection and construction previews remain
live. Transparent padding is cropped and empty fragments are discarded before
blending. Building order is cached; offscreen geometry is skipped conservatively,
including the full reach of a column or roof into the window.

A single analytic ground pass replaces the large empty-tile sweep. The world is
still open horizontally, with the same river and editable road/rail junctions.
The Afterimage feedback, full-resolution component field and Aftertaste diagram
remain. No simulation rule, world limit, save format or demo layout changed.

F3 toggles an FPS and frame-time display. Normal play retains the 60 FPS cap.
The uncapped --render-benchmark FILE diagnostic measures five paused scenarios
and verifies that rendering leaves the authored city unchanged. Optional
--screenshot PATH exports each scenario. See VALIDATION.txt for measurements;
rendering FPS does not measure the cost of an advancing simulation tick.

PREVIOUS DENSITY, DEMO AND CONSTRUCTION GUIDE

YORU / TOKYO 2091 — 1.9 / THE COMPACT VERTICAL CITY
A city without an end. Native C++17 / macOS arm64.

DENSER AND MORE EFFICIENT / NEW IN 1.9
The demo now fills every stratum of its inhabited towers, up to L12. The six main
megastructures contain 52 occupied floors, with inhabited interchange towers and
closer street frontage. A 77-by-35 ground extent replaces the 89-by-49 layout;
shorter bridges and upper walking loops bring neighbouring districts together.

Homes share floors with food markets, gardens, workplaces and rotating clinics,
schools, arcades and service workshops. Farms sit on alternate floors, with additional L6/L12 food floors and Critical
production priority. Markets open with commissioned local stocks; replacement food
is grown and transported through the ordinary finite networks. Paired pressure stages, rain recovery and local
waste-water loops supply the denser neighbourhoods. Four tall towers generate
power on L6 as well as using their ground yards. Bearing lines at the edges carry
the added floor loads through ordinary reinforced columns and foundations.

Local passenger lifts use tier two; express lifts retain direct crown connections.
Goods lifts and loading streets now serve every occupied floor, keeping cargo
off passenger transfers. The L2 arterial and L4 railway remain physical, editable
networks.
Six separate utility districts retain real capacity, head, reserves and wear.
Opening-state comparison with 1.8: about 4.5 times the population density,
34% lower mean work-route cost, 35% less delivered power per resident and 15% less
delivered water per resident. These are measured game-model quantities on the
authored presets; they change as the city grows and you edit it.

Route finding caches the fixed entrances of each freshly built network and skips
providers in disconnected components before searching. Residual capacity, head,
accessibility and provider order still govern every delivered journey or quantity.
This changes the work required by the engine, without relaxing simulation rules.

Start Yoru Megacity.command opens --demo --fit. The dense city has its own
megacity-dense/city-v6.json. Your normal city and the 1.8 megacity-demo save remain
available at their original paths. Reopening the launcher retains your edits;
--demo --fresh-city explicitly regenerates this dense preset. New city / Dense
vertical demo also creates it, retaining the existing dated backup first.

F fits the skyline; [ and ] select strata; T shows a single process floor. Use
Cut above to expose lower structures, N to build, and Space to run. The demo starts
paused and remains an open-ended city with every programme available immediately.

PREVIOUS DEMO AND CONSTRUCTION GUIDE

YORU / TOKYO 2091 — 1.8 / THE TWELVE-LEVEL CITY
A city without an end. Native C++17 / macOS arm64.

THE LARGE VERTICAL DEMO / NEW IN 1.8
Open Start Yoru Megacity.command to enter a much larger, taller Tokyo. This uses
an independent megacity-demo/city-v6.json beside the normal city directory; your
existing city and checkpoints stay available. The launcher reopens your edited
demo on subsequent visits. --demo --fresh-city explicitly regenerates that demo.
The in-game New city menu also offers Large vertical demo, Compact starter and
Blank sandbox, retaining the existing dated backup before replacement.

Six inhabited megastructures and two narrow interchange towers occupy a connected
89-by-49 ground footprint. Neighbourhood platforms sit at L1, L3, L6, L9 and L12,
with a separate L10 crown on the Akihabara stack. Heights vary across the skyline;
all homes, machinery, gardens, decks, piers and transfers remain editable.

L1 and L3 have freight streets linked by goods lifts to the L2 arterial. Road
ramps bring that arterial down to the outside avenues. The L4 maglev climbs over
four physical ramp links and serves three aerial interchanges. L6 catwalks form
upper walking loops. Skybridges and moving walks join actual concourses at several
heights, using intermediary towers to break the cross-river journey into spans.
Passenger lifts stop on every stratum; express lifts jump to the upper crowns.
Passive stairs, ground approaches and escalators provide other public routes.

Each neighbourhood has real power and water yards, food gardens, markets, service
crews and reserves. Six independent utility districts keep supply local while
roads, rail and people cross between them. A dedicated traction yard feeds the
central aerial railway and its surrounding district. Paired risers carry finite utility supply up the towers;
pressure boosters at L3, L6 and L9 lift actual inlet water to the higher homes.
The demo uses the same support, capacity, route, wear and finance rules as an
ordinary city. Free Build is on and the city starts paused.

F fits the full skyline. [ and ] select strata, T opens the floor plan, and the
Full stack / Cutaway control reveals the lower structure. Right-drag pans and
the wheel zooms. N opens construction and circulation tools. Space runs the city.

PREVIOUS CONSTRUCTION AND SIMULATION GUIDE

YORU / TOKYO 2091 — 1.7 / STRUCTURE AND CIRCULATION
A city without an end. Native C++17 / macOS arm64.

STRUCTURE AND CIRCULATION / NEW IN 1.7
N now opens Structure / materials, Walkways / transfers, and Reusable frames.
6 paints an ordinary slab; N exposes the other forms. 9 selects a two-corner
platform using the material selected in N. Click two corners on the same elevated
plane to fill up to 32 by 32 tiles. A whole platform is one undoable edit.

FOUR WAYS TO CREATE GROUND ABOVE GROUND
Concrete slab: 70 abstract load units, reach 4 tiles, own weight 2.
Truss deck: 95 units, reach 8, weight 4. Longer contiguous terraces and beams.
Transfer podium: 160 units, reach 4, weight 8. A heavier bearing floor for
    buildings and for upper columns offset from the foundations below.
Open catwalk: 24 units, reach 6, weight 0.5. Includes a public walkway and can
    carry conduits. It cannot carry buildings, vehicle roads or rail.
Structure tiers and condition still multiply capacity; reinforcement does not
change span. Materials can be converted without resetting wear or inventories.
A catwalk conversion is rejected if the tile already carries a building or road.

7 still builds a complete ground-to-level core. Individual pier builds one column
segment. It may start on ground, another column, or a transfer podium immediately
below. Upper piers therefore need not align with the ground foundations: their
weight passes down into the podium, across that floor, and through the actual
lower cores. A missing or overloaded transfer floor affects all dependent levels.
The bearing overlay and outage probes trace those offset load paths as well.

MORE WAYS TO MOVE BETWEEN STRATA
Truss skybridge: an elevated span of 2–12 tiles, straight or 45-degree diagonal,
    between existing decks on one floor. Passive capacity 140 people per tier.
Moving walk: the same bridge geometry. Draws 3 MW per tier and carries up to
    220 people per tier, with half the ordinary distance cost. Without power it
    remains a slower passive walkway with 90 capacity per tier.
Escalator: one level over 1–3 straight tiles. Draws 3 MW per tier, with up to
    160 capacity. Without power it becomes a 60-capacity stair route. It never
    provides step-free access, including while running.
Pedestrian ramp: one level over 2–6 straight tiles, 110 capacity. A passive
    step-free route, with no vehicle, rail or public utility connection.
Express lift: two or more strata in one vertical column, with doors only at its
    two stops. Draws 4 MW per stratum per tier; base capacity is 70 divided by
    1 + 0.08 times the number of additional strata. Travel costs 2 plus one unit
    per three strata, rounded up. An intermediate concourse does not acquire a door.
Existing stairs, every-floor lifts, road ramps, goods lifts and risers remain.
All capacities retain condition/tier scaling, shared use, queue costs and wear.
Moving walks and escalators need a real electrical feed at their lower/first
landing. Express shafts use their base feed. Reserves and priorities still apply.

Bridges, escalators and pedestrian ramps require existing elevated landing decks;
they add no hidden ground cores. Express lifts include a continuous shaft core.
New spans add structural load to their anchors. Bridges connect at their two
landings, not at the tiles beneath them, and reserve their own air corridor.
Same-level bridge crossings and overlaps are rejected; different levels can cross.
Use ordinary truss deck cells when you want an occupiable floor with intermediate
junctions or a physical conduit route instead of a two-landing public span.

REUSABLE STRUCTURAL FRAMES
N / Reusable frames and the second page of B contain three editable, empty kits:
Terrace frame: a public transfer podium on four cores, with a pedestrian ramp.
Bridge court: separate elevated courts joined by a truss bridge and moving walk,
    with ramp/escalator approaches and utility feeds.
Offset transfer frame: truss floors on upper piers that start on a lower podium,
    offset from the ground cores; pedestrian ramp and escalator circulation.
Frames place actual structure and routes, without residents or programme modules.
Add housing, employment, food and services to make them neighbourhoods. Existing
blueprint indices and the original starter quarter remain intact.

Schema six stores deck forms and the appended connection kinds. A missing
city-v6.json imports v5, then v4, v3, v2 or v1, preserving all source files.
Old decks remain ordinary slabs; no new structures are inserted into old cities.


THE RESILIENT MACHINE / NEW IN 1.6
M / Resilience has Ground routes, Pipe circuits, Protected reserves and Outage
probe pages. LEAKS and EXITS join the existing overlays in both city views.
The BEARING overlay now draws arrows for the actual routed structural loads.

DEPENDENCIES AND FAILURE
Households expose up to two edge-disjoint ground routes. These paths cannot
share a walkway, stair, lift or ramp link, including their approaches. Two lifts
behind one shared approach still count as one route. The separate blackout
readout excludes all powered lifts and elevated rail-station arrivals. Passive
stairs can preserve general access; a passive ramp can preserve step-free access.
Reachable public ground is the destination; existing ground-level paths are
not treated as additional vertical exits. EXITS labels show route count plus R (passive step-free), S (passive stairs) or
L (powered access required). This is a game network rule, not a safety standard.

Use Probe in M / Vertical, Probe beside a core in Engineering, or the landing
inspector to test a single failure. A probe copies the current city, removes that
transfer or core segment, rebuilds support and every utility/transport network,
and compares access, support, utility delivery, work journeys and local output.
It consumes no stock, advances no time and changes no authored tile or resident.
Results are labelled snapshots: re-run after edits or ticks. Removing a foundation
can affect many floors, while a second bearing path can carry a surviving stack.
The comparison lists all components that lose service under the selected failure.

PIPES ARE MAINTAINED MACHINERY
Each pipe tile has persistent condition and an isolation valve. Wear reduces its
capacity. Below 85% condition it leaks a share of the inlet water on every routed
edge; the lost volume is removed before the next edge. Pumps, storage discharge,
charging and final delivery obey the same loss accounting. The pipe table and
LEAKS overlay show actual lost kL per tick at the damaged cells, not an arbitrary
warning effect. Unused source potential remains separate from dispatched water.

Isolate a pipe before a full manual repair, then reopen its valve. A valve cuts
that tile out of the water graph, including a building outlet on the same tile.
Loops can keep the other side supplied while a branch is closed. Workshops can
repair reachable pipes during operation, sharing the same finite parts-fed crew
budget as buildings, connectors and structures. Isolated pipes still age slowly.
Removing a pipe also removes its valve and resets that pipe's condition.

PHYSICAL EMERGENCY RESERVES
Batteries and reservoirs can protect 0, 25, 50 or 75% of their total capacity.
Normal and Low priority demand can use only stock above that protected amount.
Critical consumers, including pumps and transfer motors, can release the reserve;
they still need a connected route and remain subject to discharge rate limits.
The reserve is a minimum stock threshold, not extra energy or water. Charging
uses the existing efficiencies and never occurs in the same tick as discharge.

An elevated reservoir can feed its own level and lower routes by gravity even
without electricity. Electrical service adds one stratum of delivery head.
Charging still needs power. A ground reservoir needs powered pumping. Build
upper water storage and an appropriate battery circuit to make a district less
dependent on a single booster, riser or ground-level utility plant.

HOUSEHOLDS REMEMBER THEIR JOURNEYS
Missed care and leisure persist as separate bounded backlogs, increasing later
visit demand. Clinics serve accumulated need and poorer health before equal
needs, rather than permanently favouring map order. Actual appointments and
routes remain finite. Backlogs clear gradually as visits are delivered.

Mean work-route cost includes real stairs, lifts, distance and remembered queue
delays. Long journeys and missing recovery time accumulate fatigue on city ticks.
Fatigue and health reduce the available 62% working share; they do not create a
second workforce. Household wellbeing and health also reflect those persistent
conditions. All memory stays still while paused, including during outage probes.
The People panel and household inspector expose route cost, fatigue and care
backlog alongside employment, health and visits.

Schema five stores pipe valves/condition, reserve settings and household memory.
Missing city-v6.json imports v5, v4, then v3, v2 or v1, leaving those files unchanged.
Old pipes start sound/open; new household memory and reserve fractions start at
zero. Authored streets, programmes, inventories, residents and transfers stay put.


THE INHABITED MACHINE / NEW IN 1.5
M / People exposes care, leisure, health, employment and step-free entrances.
M / Engineering traces weight through cores and foundations and exposes water
head deficits. ACCESS, BEARING and HEAD overlays work in both city views.
8 paints reinforcement; the landing inspector upgrades, services and prioritises
transfers. These constraints apply in Free Build as well as City Economy.

DAILY LIFE THROUGH SPACE
18% of workers and service demand use a step-free profile. They can travel along
ordinary pavement, public highwalks, ramps and powered passenger lifts, but never
stairs. The remaining 82% can use stairs. These are fictional planning shares.
All workers share the original 62% resident workforce; nobody is assigned twice.
Rail-connected powered stations and L0 exchanges provide network entrances.
An upper household can reach local destinations even if its only outside route
is a stair, provided its journey itself has a continuous step-free path.

Care and leisure are routed to actual working venues, after commuting and goods
have used their shared pedestrian slots. A return visit consumes two slots per
person; disconnected or full routes block appointments. Care costs at most 24
route units each way; leisure 20. Each tick, care demand is 1% of residents and
leisure 1.5%. Nominal venues serve 12 clinic visits, 20 arcade visits, 8 shrine
visits, 12 garden visits or 8 market visits, scaled by density and productivity.
Care journeys are pink, leisure gold, step-free work blue and general work cyan.
Health persists and changes gradually with actual care, food, water, sanitation
and pollution. Health, care and leisure contribute to residential wellbeing.

BEARING PATHS
Decks carry their own weight, installed transport and actual programmes. Heavy
water and industrial modules weigh more than housing. Loads split equally over
shortest deck paths to cores, with a maximum four-tile span. Each column segment
and foundation carries all the floors above it. Tier-one decks bear 70 abstract
load units; cores bear 240. Structure tiers multiply capacity by 1, 2 or 3, and
condition reduces it. These are game rules, not engineering specifications.

Above 100% load, dependent output is derated; above 175% the affected upper
network and modules close safely. Authored tiles remain editable. Overload
accelerates structural wear. Reinforce the whole load path, including foundation,
or distribute programmes among more cores. The inspector and Engineering panel
can strengthen an entire column in one action. Tile reinforcement also upgrades
installed roads, walkways, cables and pipes. Manual structural repair restores
condition; parts-fed workshops share their finite crew budget between worn
buildings, transfer machinery and reachable structural members.

WATER MUST CLIMB
Waterworks reach L(source+3), reclaimers +1, recovery plants +2 and reservoirs +1;
source density upgrades extend treatment-source reach. Every point of a delivery
path must remain below its source head, including humps above a low destination.
The new Pressure booster consumes electricity and transfers up to 100 kL/tick of
existing inlet water to a head three strata above itself. Density upgrades extend
its reach. Put it at a landing the previous source can reach, then cascade more
boosters upward. It creates no water. Stage power and risers as well as pumps.
Original supply and boosted throughput are shown separately; boosted throughput
may count the same water again at each cascade. All stages share actual pipe
capacity. Pumps now respond to actual downstream demand during priority dispatch,
drawing enough inlet water to cover the measured losses at each cascade stage.

TRANSFER MACHINERY
Connectors have three capacity tiers and persistent condition. Capacity scales
with tier and condition; passenger and goods motor demand scales with tier.
Transfers below 12% condition stop. Passenger and goods lifts also stop below
40% electrical service. Critical, Normal and Low priority controls apply to
motors. Used machinery wears; a remembered load adds queue delay to route cost
and can push long journeys beyond their travel budget. Extra flights or parallel
routes improve resilience. The landing inspector offers Upgrade and Overhaul.
All persistent ecology fields save in schema six. A missing city-v6.json imports
v5, v4, then v3, v2 or v1 without changing the originals or adding new city components.


BUILD UP AS WELL AS OUT
The city now has thirteen real spatial strata, L0 through L12. Every tile,
component, route, inventory and shipment has an (x,y,z) address. A road at L2 does
not connect to the one beneath it. Crossings at different heights have separate
capacity and require an explicit transfer. Density upgrades remain a separate
property of the module; they do not change its spatial address.

N opens the vertical construction tools. [ / ] or Page Down / Page Up selects
the active level. 5 paints highwalks, 6 decks, 7 bearing cores. T changes between
the Afterimage machine view and an Aftertaste floor plan of the selected stratum.
In the machine view, Full stack / Cut above exposes or hides upper levels. Home
frames the selected component and its actual level. M / Vertical lists every
transfer, its landings and measured dispatch; Locate takes you to that landing.
The PEOPLE overlay shows costed commuter routes in either view. Freight routes
also retain their real elevations, including in single-floor diagram slices.

DECKS, CORES AND PUBLIC GROUND
Upper modules need a deck. A continuous bearing core reaches from L0 to the
selected level; supported deck tiles can span four connected deck tiles from a
core. Unsupported cells remain editable but their machinery and routes do not
operate. Removing a foundation severs support above; undo restores the complete
structure. Loads now travel through decks and continuous columns to foundations. See the bearing rules below.

Paint highwalks over decks, or through a building to make a public concourse.
Private building footprints do not automatically become through-routes. The same
plan position can hold a street, housing, employment and a roof garden on separate
strata. All 28 programme modules can be placed on supported upper decks.
These are coarse bands of programmes, not literal single-storey floor heights.

EXPLICIT TRANSFERS
Choose a connector in N, click its lower landing, change level with [ / ], then
click the upper landing. Q cancels. Each connector has real endpoints:
  Stairs: adjacent levels, same or neighbouring plan tile. Passive; 60 people/tick.
  Passenger lift: one vertical column, with intermediate landings. 90 people/tick
      per flight when fully powered; each flight draws 4 MW from the base feed.
  Goods lift: a separate shaft for loading streets. 24 goods/tick per flight;
      each flight draws 8 MW. It does not act as a public passenger connection.
  Road ramp: rises one level over 3–8 straight tiles. Separate carriageway and
      pavement capacity. Its sloping corridor must remain clear of buildings and
      columns. No junction is invented partway along the slope. Rail or conduits
      can use the ramp when installed at both landings.
  Utility riser: explicit power and water links. It carries no people or freight.

Powered transfers stop below 40% electrical service. Passive stairs and ramps
remain available during a blackout. A continuous lift shaft takes its motor feed
from its lowest installed flight; it does not distribute public utility service
to surrounding buildings. Use separate risers and local conduits for that.
Connectors include structural landing support, not invisible links to another
network. Closing a connector leaves its landings and supporting structure in place.

MOVEMENT HAS CONSEQUENCES
Pavements and highwalks form a distinct pedestrian graph with 180 people/tick of
base horizontal link capacity. Workers take bounded, costed routes from homes to
jobs, sharing actual stairs, ramps and lifts. Stair flights cost five route units,
lifts three, and ramps their horizontal length plus two. The commute budget is
40 route units. A resident cannot staff two jobs in the same forecast. Technical
institutes and workshops also use costed pedestrian reach for their local service.
The autonomous staffing floor remains, but upper public destinations require a
reachable pedestrian entrance. Rail-connected stations can anchor pedestrian
arrival; operating freight terminals can anchor loading streets.

Food reaches homes through markets over the pedestrian last mile. Light goods
and waste carts can fall back to public routes, using six pedestrian capacity
slots per unit of goods. Scrap and alloy require loading streets and a real road
ramp or goods lift. Workers are dispatched before courier traffic. Utility
machinery can run autonomously once commissioned, but isolation limits staffing
and maintenance. Power and water only move between floors through installed
spatial links; the same plan coordinate is not enough.

A REUSABLE VERTICAL QUARTER
N or B offers a complete multi-level quarter with podium homes, markets, offices,
workshops, farms, public highwalks, a roof garden, a road ramp, passenger and goods
lifts, stairs and risers. Place this plan on L0 beside an existing service street.
It is real editable construction: one undo reverses its entire spatial footprint.
The new starter city includes the quarter west of the original neighbourhoods.
Existing cities retain their layouts; add the plan or build vertical routes freely.

RESEARCH BASIS
Hong Kong's integrated building-to-building walkways and hillside transfers,
London's Barbican podium/highwalk public realm, and Newcastle's MEA House public
deck and suspended structure inform the system. See VERTICALITY-RESEARCH.txt for
linked primary sources and the distinction between research and fictional rules.
Neither the city nor its engineering dimensions reconstruct those real places.

SAVES / MIGRATION
Spatial state uses city-v6.json and checkpoint-v6.json in the existing independent
YoruTokyo data folder. When no spatial save exists, the app imports city-v5.json, then city-v4.json, then city-v3.json, city-v2.json,
or the original city.json if necessary. Original saves remain unchanged. Legacy
coordinates become L0; no elevated structures or connections are invented.
Checkpoint restore also falls back to older checkpoint formats. Atomic
saves retain a backup; malformed and future saves remain protected.

All stock, production, trade, wear, education, policies, storage and economy
systems from 1.3 remain in the shared model. The research sources and game rules
are separate: capacities, support spans and travel costs are fictional parameters.

MACHINE ECOLOGY / RETAINED SYSTEMS

THE CITY IS A MACHINE
A separate, open-ended cyberpunk city builder descended from Aftertaste's native
construction sandbox, with an Afterimage-inspired optical machine view and an
Aftertaste-style top-down process diagram. T switches views; both edit one city.
28 components and every system are available immediately. No objectives, timers,
research gates or end state. Tokyo is a fictional compressed interpretation.

Open Yoru Tokyo.app. Start Yoru Diagram.command opens the complete diagram.
Tab opens the library and inspector. M opens the machine kernel: trace a process,
inspect local inventories and material balance, locate saturated links, change
district regimes, control trade and read operating history. Select a component
and follow its incoming/outgoing shipments to trace an entire production chain.

MATERIAL ECOLOGY
Seven goods exist as quantities at individual buildings: food, scrap, alloy,
chips, parts, waste and nutrients. Recipes consume real buffered input and stop
when an input is missing or an output store is full. Production becomes available
for dispatch next tick. Every forecast obeys stock + production + imports -
consumption - exports. Inspection and switching views never commit a forecast.

Per nominal level-one cycle, before workforce, condition and workload modifiers:
  Farm:        0.15 nutrients -> 23 food
  Recycler:    8 waste -> 4 scrap + 2 nutrients
  Smelter:     4 scrap -> 2 alloy
  Cleanroom:   1 alloy -> 1.5 chips
  Foundry:     1.4 alloy + 0.35 chips -> 1.8 repair parts
  Workshop:    0.5 parts -> up to 0.08 aggregate component condition repair
  Recovery:    6 waste -> up to 70 kL clean water + 0.8 nutrients
Actual recovery conversion follows dispatched water. Unused treatment capacity
does not consume waste. Households eat 0.1 food per resident per tick and produce
waste. Working employers also produce waste. Uncollected waste accumulates at
its source, lowers hygiene and adds nearby pollution; it does not vanish.

Buildings include a small commissioning kit in their construction: eight base
cycles of non-waste recipe inputs, household food and market starter food.
Construction plans include the same kit. Upgrades do not refill inventories.
Version-one cities receive this kit once when migrating to the deeper model.

PHYSICAL LOGISTICS
Streets must reach a City exchange. Food does not teleport between districts:
farms, gateways and warehouses supply markets, which distribute to homes.
Markets can dispatch 36 food per nominal cycle. Households hold a local reserve;
farms and industrial processes keep separate input and output buffers.

Every shipment follows an actual shortest available street route. Road links
share 64 units of freight capacity per tick; each building has a finite dock.
Alternate paths can carry flow when a route fills. Warehouses collect remaining
goods after direct consumers and redistribute from their own physical stock.
The FREIGHT overlay shows the next tick's planned dispatch and saturated roads.
Selected components reveal their own scheduled shipments in the normal diagram.
Material priority orders consumers within each intake stage; process and market
buffers fill before households, then warehouses collect remaining goods.
Stock already produced can be collected even while its producer is not running.

Exchange imports are capped at 24 units per nominal cycle. Intermodal freight
terminals offer 55 and require operating utilities plus rail to an exchange.
Imported goods enter gateway inventories and still need road delivery. Imports
rotate across goods and respect available money in City Economy. Disable imports
to attempt a circular local economy. Optional exports protect working reserves,
exclude stock at other gateways and sell below import price. Warehouse stock has
no provenance tracking; re-exporting imported stock cannot yield a trading profit.

UTILITIES AND STORAGE
Power and water use separate physical graphs. Components have independent service
priorities: Critical, Normal, Low. Utility dispatch shares each tier over eight rounds
before moving to the next. Each routed link has a finite capacity: 160 MW for
cables and 140 kL per tick for pipes. Components can receive from multiple sources
and residual paths. These are capacity-constrained service routes, not an AC
power-flow or hydraulic pressure solver. No transmission losses are invented.

Fusion supplies up to 240 MW; solar up to 48 with a 144-tick day/night cycle.
Waterworks and reclaimers consume electricity. Recovery plants consume actual
waste to supply clean water. Batteries hold 480 MWh and discharge up to 48 MW;
reservoirs hold 600 kL and discharge up to 60. A tick is ten simulated minutes;
battery energy converts MW to MWh over that interval. Surplus charges stores after
consumer demand. Battery charging is 90% efficient, water storage 98%. A store
never charges and discharges in the same tick. Reserves survive saving/loading.

Click a street, cable or pipe tile and reinforce its installed layers. There are
three tiers; each extra tier adds 125% of base link capacity. Both ends determine
a link's tier. Reinforcement and extra parallel routes solve different bottlenecks.
Use MW LOAD and kL LOAD overlays or locate a loaded link from the kernel.

PEOPLE, SKILLS AND WEAR
Pedestrian routes assign residents to reachable workplaces. Residents supply up to a 62% working
share, reduced by health and fatigue; employers share that labour. An autonomous 35% staffing floor permits a
small district to bootstrap. Utility machinery is autonomous. Data towers,
cleanrooms and foundries also need skilled staffing, derived from the education
of the workers who actually reach the workplace. Technical institutes gradually train homes
within 18 pedestrian route-cost units, sharing finite teaching capacity. New arrivals bring a
baseline education and dilute the existing average. Commuter batches use finite physical pedestrian routes; maglev improves nearby residential wellbeing.

Condition wears down each real tick and persists. Low condition reduces output;
below 12% a component shuts down mechanically. Workshops consume parts and share
finite repair capacity among worn components within 18 pedestrian route-cost units. A manual field
overhaul restores condition at a credit cost in City Economy. Disabled machinery
still ages slowly. Higher workload raises output, draws more utilities and wears
faster. The inspector and kernel expose the controlling factors and bottleneck.

District operating regimes compound component workload:
  Balanced:  100% output, 100% utility draw, 100% wear, 100% waste
  Circular:   85% output,  72% utility draw,  65% wear,  55% waste
  Overdrive: 125% output, 140% utility draw, 190% wear, 140% waste
Regimes are set independently for the named Tokyo wards in M / Districts.
Education, condition, reserves, buffers, regimes, priorities and history all save.

OPEN CONSTRUCTION
The starter city opens paused. Drag street, electricity, water or maglev to paint
connected strokes. Layers coexist on a tile; cables and pipes run beneath buildings.
Streets and rail bridge the Sumida; buildings need dry ground. Found a remote ward
with its own exchange. Free Build waives construction and financial constraints;
recipes, physical capacity, stock, workforce and wear still operate. City Economy
adds construction and upkeep, taxes, import costs and export receipts. Debt has
no loss condition. Switch back to Free Build at any time.

B opens seven editable construction plans across two pages, including three structural frames and a vertical quarter. Plans place actual buildings and service
spines, not people or simulation state. The sparse world extends from -1,000,000
to +1,000,000 in both axes. Undo groups each stroke or plan until the next tick.

OPTICAL INSTRUMENT
The main view adapts Afterimage's spectral fields, bounded feedback and bloom to
actual component activity. Installed power, water and rail routes become braided
filaments. O selects a calmer optical treatment. UI stays sharp outside feedback.
The diagram is a crisp white process floor with equipment symbols and real service
ports. Pulses on ordinary service buses indicate availability; load overlays and
the kernel give measured routed quantities. Freight dots indicate planned routes.
Optical material keeps drifting while paused; real process markers hold still.

CONTROLS
T machine/diagram; Tab workbench; M machine kernel; O full/calm optics.
Space run/pause; period single tick; 1/3/8x controls simulation speed.
1 street, 2 electricity, 3 water, 4 maglev; Q inspect/cancel; E clear.
B plans; V cycles fifteen overlays; H/F1 guide; Esc closes or cancels.
Wheel zoom around pointer; right/middle-drag or Alt-left-drag pan.
WASD/arrows pan; F fit whole city; Home frame selected component in diagram.
Select a building to inspect inputs, buffers, wear, workload, priority, upgrade,
switch off, overhaul or remove it. Select a network tile to reinforce its layers.
U or Cmd/Ctrl-Z undo construction/control edits before the next tick.
Cmd/Ctrl-S checkpoint; Cmd/Ctrl-L restore checkpoint paused. Click minimap to travel.

SAVES
macOS: ~/Library/Application Support/YoruTokyo/
Windows: %APPDATA%/YoruTokyo/
Linux: $XDG_DATA_HOME/yoru-tokyo/ or ~/.local/share/yoru-tokyo/
YORU_DATA_DIR overrides the independent folder. No Aftertaste saves are touched.
The spatial model uses city-v6.json and checkpoint-v6.json; legacy city files
are imported only when no spatial save exists, preserving their originals.
Autosave runs every 35 seconds and on clean exit. Atomic saves keep .bak files.
Invalid or future saves remain untouched and autosave is blocked until recovery.
A manual recovery checkpoint is still possible. New cities first save a dated
backup. Loaded cities open paused. View mode and optics are not simulation state.

BUILD / VALIDATION
./build.sh
Or cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j 2
   ctest --test-dir build --output-on-failure
Headless model: -DYORU_BUILD_APP=OFF. Sanitizers: -DYORU_SANITIZE=ON.
Native QA: --ui-test --render-check --diagram-check --depth-check --vertical-check --ecology-check --resilience-check --construction-check
Rendering: --screenshot /absolute/path.png --size 1600x1000 --fresh-city --fit
--diagram opens the process floor. --ticks N advances before showing it.
Use an isolated YORU_DATA_DIR for --fresh-city, --blank or diagnostic runs.

LINEAGE AND LICENSES
Aftertaste supplies the native C++/raylib construction-sandbox lineage; Afterimage
supplies the optical reference and float phase-buffer technique. Yoru's city model,
production, logistics, wear, skills and machine kernel are independent. Neither
original project is modified. See ORIGIN.txt and VALIDATION.txt.
raylib 5.5: zlib (vendor/raylib/LICENSE).
nlohmann/json 3.11.3: MIT (vendor/json-LICENSE.txt).
System fonts load when available; the built-in raylib font is the fallback.
