# Third-party assets

## Pacific Coast wildlife

The wildlife assets below are used under the Creative Commons Attribution 4.0 International license. AeroSim modifies their import settings, scale, materials, placement, and runtime presentation. The original source archives are not committed; the Unreal Engine assets under `Content/ThirdParty/Wildlife/` are derived from the listed downloads.

| Asset | Creator | Source | License | Downloaded | Source SHA-256 |
|---|---|---|---|---|---|
| Animated Bear 3D Model | AnimalMesh 3D (`AnimalMesh3D`) | https://sketchfab.com/3d-models/animated-bear-3d-model-3adff88544344a58a8c93b10aaf05451 | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) | 2026-10-27 | `ca8d8d929df51410732c1612cb9f47b7f6b598e2199bda5ac0dabcbea34e63e7` |
| Deer Animation | DarryleH (`DarryleH`) | https://sketchfab.com/3d-models/deer-animation-347e5d4fbee14e53811c5df2221be8d0 | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) | 2026-10-28 | `bf554a3c4a8b3684af8886427293e8def35ed2af7e21296eaf6903d77007bb5c` |
| Humpback Whale (Swimming) | Connlan Immure (`Connlan_Immure`) | https://sketchfab.com/3d-models/humpback-whale-swimming-f4912be9163a4f45b2480df8ccb8b2c6 | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) | 2026-10-27 | `f7f5480b890ca944933f3af11b240cb5d7281050cc8d197bd22522856e1c4f37` |
| animated EAGLE | Asim3d (`Asim3d`) | https://sketchfab.com/3d-models/animated-eagle-8fb150270adb45aa9653e857f3fa351b | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) | 2026-10-28 | `eab311f0b33813b1cafad77027d5a65c2806a8795f95d7e9304103ce8f216559` |

### Modifications

- Imported source FBX/GLB files into Unreal Engine 5.8 skeletal meshes and animation sequences.
- Rebuilt the whale material from the supplied base-color, roughness, normal, and ambient-occlusion textures.
- Adjusted world scale, orientation, animation phase, culling distance, collision, habitat placement, deterministic roaming, proximity alert, fleeing, separation, and eagle attack presentation.
- Retained whale, bear, deer, and eagle in the Pacific Coast acceptance habitat; wolf and cattle were removed.

License text: https://creativecommons.org/licenses/by/4.0/legalcode
