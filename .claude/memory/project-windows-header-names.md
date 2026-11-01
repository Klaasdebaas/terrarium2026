---
name: project-windows-header-names
description: Bestandsnamen in terrarium/ mogen niet hoofdletterongevoelig gelijk zijn aan headers van de ESP32-core
type: project
---

Geen sketchbestanden in `terrarium/` met namen die (zonder op hoofdletters te
letten) gelijk zijn aan core-/library-headers zoals `Network.h`, `WiFi.h`,
`WebServer.h`, `FS.h`, `SD.h`.

**Why:** De sketchmap staat vooraan in het include-pad en Windows is
hoofdletterongevoelig. `network.h` overschaduwde de `Network.h` van de core,
wat de hele WiFi-library brak (opgelost door hernoemen naar `net_wifi.h`).
Op Linux (host-tests, cloud) is dit onzichtbaar.

**How to apply:** Bij nieuwe modules een unieke prefix/naam kiezen; slagende
host-tests bewijzen niets over dit soort Windows-only buildfouten.
