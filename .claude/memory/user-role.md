---
name: user-role
description: Wie de gebruiker is en hoe die de firmware bouwt en flasht
type: user
---

Nico schrijft in het Nederlands. Bouwt en flasht zelf lokaal op Windows met de
Arduino IDE (esp32-core 3.3.x, board ESP32C6 Dev, PartitionScheme=minimal,
CDCOnBoot=cdc). Haalt de code vaak binnen als zip-download van `main` in
`Downloads\terrarium2026-main`, dus wijzigingen op een feature-branch zitten daar
niet automatisch in.

**How to apply:** Claude kan niet voor de ESP32 compileren; vraag om de build-log
en geef bij fixes expliciet aan welke branch/zip hij moet ophalen of welke lokale
stappen nodig zijn (incl. IDE herstarten voor de build-cache).
