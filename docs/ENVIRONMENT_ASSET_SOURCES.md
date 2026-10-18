# Development coastal texture sources

State: DRAFT_NOT_USER_CONFIRMED. These textures and locally authored materials are confined to `/Game/Development/CoastDraft`; this is not formal aircraft/content delivery, production promotion, AAA acceptance or user appearance confirmation.

- [Pine Tree 01](https://polyhaven.com/a/pine_tree_01), Poly Haven; photography Rob Tuytel, modeling Rico Cilliers. Seven original 1K PNG texture maps only, provider MD5 verified. No 17-million-triangle provider tree mesh was acquired. Tree skeletons, placements and needle cards are project-authored. A real needle island was isolated by connected alpha, cropped, resized and padded only outside its opacity mask. Original source and derivation hashes are in `SourceAssets/EnvironmentDraft/PineTree01/source-manifest.json`.
- [Rocky Terrain 02](https://polyhaven.com/a/rocky_terrain_02), Amal Kumar / Poly Haven: 2K base color, DirectX normal and roughness. Publisher scale is **90 m**, used as a macro terrain surface; source texture details do not create actual pebble collision or vegetation geometry.
- [Cliff Side](https://polyhaven.com/a/cliff_side), Poly Haven: 2K base color, DirectX normal and roughness, **1.8 m** world projection.
- [Poly Haven asset license](https://polyhaven.com/license): CC0. Download receipts preserve publisher URLs, file sizes, MD5 and SHA256. Scripts acquire only fixed publisher assets with an identifying User-Agent and bounded file sizes.
- Water ripple normal is the bundled UE 5.8.3 `water_n` example texture, used under the engine distribution terms. It is not claimed to be a newly authored or CC0 texture. The opaque PBR ocean has animated surface normals; it is not a fluid simulation, volumetric water, breaking-wave model or sea-level flight validation.

`docs/environment-draft-assets.json` freezes generated engine asset and source image hashes. Historical no-binary checks now allow only this fixed hashed Phase7 development set; unmanifested assets, changed source hashes, unknown licenses and production states fail the policy tests. No formal map is edited. Source import filenames are stored project-relative, with no local user directory embedded.


## V8 gravel intake and current water material

[Sandy Gravel 02](https://polyhaven.com/a/sandy_gravel_02), Dario Barresi / Poly Haven, [CC0](https://polyhaven.com/license): original 2K base color, DirectX normal and roughness JPG maps only, total 11,141,745 bytes. Provider MD5 and SHA256 are verified in SourceAssets/EnvironmentDraft/GravelDraft/source.json. The provider dimension value is millimetres: 2529.9999713897705 mm gives approximately 2.53 m world repeat, not 2530 metres. At 2K this is approximately 1.235 mm per texel. This does not establish site identity, engineering compaction, geometry displacement/collision or production visual acceptance.

M_CoastGravelDraft blends those maps with the unchanged macro land material using vertex alpha on the launch apron/access and forest trail. The 22-asset fixed development whitelist and all source hashes are frozen in environment-draft-assets.json version coast-draft-20261017-v7. Intake is imported draft pending visual/user review; it is not formal content promotion.

The earlier opaque-water description is historical. The current material uses UE SingleLayerWater, analytic height/normal components, moving ripple normals, absorption/scattering and broken shore foam. It remains a draft analytic surface, without a spectral ocean, fluid solver or breaking-wave simulation. V8's environment clock advances in real Metal captures; earlier whole-world-paused sequences do not establish water motion.
