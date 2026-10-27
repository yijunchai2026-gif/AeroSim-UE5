# Third-party assets

## Pacific Coast wildlife

The wildlife assets below are used under the Creative Commons Attribution 4.0 International license. AeroSim modifies their import settings, scale, materials, placement, and runtime presentation. The original source archives are not committed; the Unreal Engine assets under `Content/ThirdParty/Wildlife/` are derived from the listed downloads.

| Asset | Creator | Source | License | Downloaded | Source SHA-256 |
|---|---|---|---|---|---|
| Animated Bear 3D Model | AnimalMesh 3D (`AnimalMesh3D`) | https://sketchfab.com/3d-models/animated-bear-3d-model-3adff88544344a58a8c93b10aaf05451 | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) | 2026-10-27 | `ca8d8d929df51410732c1612cb9f47b7f6b598e2199bda5ac0dabcbea34e63e7` |
| Animated Deer | igor-lir (`igor-lir`) | https://sketchfab.com/3d-models/animated-deer-4cff1e63f4e941198b3d56300b3393b9 | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) | 2026-10-27 | `296d6ef0cc008470a211b895cf01b6f3184feed43715debf852cb3d30454c574` |
| Humpback Whale (Swimming) | Connlan Immure (`Connlan_Immure`) | https://sketchfab.com/3d-models/humpback-whale-swimming-f4912be9163a4f45b2480df8ccb8b2c6 | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) | 2026-10-27 | `f7f5480b890ca944933f3af11b240cb5d7281050cc8d197bd22522856e1c4f37` |

### Modifications

- Imported source FBX/GLB files into Unreal Engine 5.8 skeletal meshes and animation sequences.
- Rebuilt the whale material from the supplied base-color, roughness, normal, and ambient-occlusion textures.
- Adjusted world scale, orientation, animation phase, culling distance, collision, and habitat placement.
- Retained only whale, bear, and deer in the Pacific Coast acceptance habitat; wolf and cattle were removed.

License text: https://creativecommons.org/licenses/by/4.0/legalcode
