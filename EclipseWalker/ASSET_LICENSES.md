# EclipseWalker Asset Licenses

This file records the asset license information that is currently available from
the repository and this Codex task history.

It is not legal advice. Assets marked `NEEDS VERIFICATION` must be verified,
replaced, or removed before a public Steam release.

## Licensed Unity Asset Store Assets

The following assets were identified by the project owner as assets used in
EclipseWalker. Each Asset Store page lists the `Standard Unity Asset Store EULA`
as the license agreement. Unity's Asset Store EULA allows licensed, non-restricted
assets to be incorporated into a larger electronic application or digital media
product and distributed as embedded components of that product, subject to the
EULA restrictions.

Unity Asset Store EULA:

- https://unity.com/legal/as-terms

Important release notes:

1. Keep purchase receipts/order IDs for each Unity account that licensed these
   assets.
2. Do not redistribute the raw asset files as standalone assets.
3. Do not let unlicensed third parties use the source asset files unless your
   Unity license tier and contractor terms allow it.
4. If a team/company owns the project, confirm whether `Single Entity` is enough
   or whether `Multi Entity` is required for your organization.
5. Unity Asset Store credits are generally not required by the Standard EULA, but
   keeping an internal asset list is strongly recommended for Steam review and
   future audits.

### Stylized Archer Skeleton

- Asset Store URL: https://assetstore.unity.com/packages/3d/characters/creatures/stylized-archer-skeleton-309042
- Publisher: ThunderForgeStudio
- License agreement: Standard Unity Asset Store EULA
- Store page license type shown: Single Entity / Multi Entity
- Commercial game distribution: Allowed as embedded content in a licensed product,
  subject to the EULA.
- Attribution required: Not generally required by the Standard Unity Asset Store
  EULA.
- Used/derived files:
  - `Models/Skeleton2/**`
  - `Textures/Archer Skeleton Classic.dds`
  - `Textures/Archer Skeleton Classic.tga`

### Stylized Warrior Skeleton

- Asset Store URL: https://assetstore.unity.com/packages/3d/characters/creatures/stylized-warrior-skeleton-309037
- Publisher: ThunderForgeStudio
- License agreement: Standard Unity Asset Store EULA
- Store page license type shown: Single Entity / Multi Entity
- Commercial game distribution: Allowed as embedded content in a licensed product,
  subject to the EULA.
- Attribution required: Not generally required by the Standard Unity Asset Store
  EULA.
- Used/derived files:
  - `Models/Skeleton/**`
  - `Textures/Warrior Skeleton Classic.dds`
  - `Textures/Warrior Skeleton Classic.tga`

### DEMON LORD

- Asset Store URL: https://assetstore.unity.com/packages/3d/characters/humanoids/fantasy/demon-lord-514
- Publisher: PROTOFACTOR, INC
- License agreement: Standard Unity Asset Store EULA
- Store page license type shown: Single Entity / Multi Entity
- Commercial game distribution: Allowed as embedded content in a licensed product,
  subject to the EULA.
- Attribution required: Not generally required by the Standard Unity Asset Store
  EULA.
- Used/derived files:
  - `Models/Boss/**`
  - `Models/Animated/Boss/**`
  - `Textures/T_DemonLordBody_BaseColor.dds`
  - `Textures/T_DemonLordBody_Emissive.dds`

### Demon Archer - Game ready character

- Asset Store URL: https://assetstore.unity.com/packages/3d/characters/humanoids/fantasy/demon-archer-game-ready-character-343896
- Publisher: Feyloom
- License agreement: Standard Unity Asset Store EULA
- Store page license type shown: Single Entity / Multi Entity
- Commercial game distribution: Allowed as embedded content in a licensed product,
  subject to the EULA.
- Attribution required: Not generally required by the Standard Unity Asset Store
  EULA.
- Used/derived files:
  - `Models/Imp/Animation/Imp_Archer/**`
  - `Models/Imp/Model/SKM_Imp_Archer.fbx`
  - `Textures/T_Demon_Archer_Base_color.dds`
  - `Textures/T_Demon_Archer_Base_color.tga`

Potentially related but still needs final source confirmation:

- `Models/Imp/Animation/Imp/**`
- `Models/Imp/Model/SKM_Demon.fbx`
- `Textures/T_Demon_Base_color.dds`
- `Textures/T_Demon_Base_color.tga`

These non-archer demon files are present in the repository, but the supplied URL
is specifically for `Demon Archer - Game ready character`. Confirm whether the
package also contains the non-archer demon assets or add the correct source URL.

### Stylized dungeon

- Asset Store URL: https://assetstore.unity.com/packages/3d/environments/dungeons/stylized-dungeon-197290
- Publisher: Sirik
- License agreement: Standard Unity Asset Store EULA
- Store page license type shown: Single Entity / Multi Entity
- Commercial game distribution: Allowed as embedded content in a licensed product,
  subject to the EULA.
- Attribution required: Not generally required by the Standard Unity Asset Store
  EULA.
- Used/derived files:
  - `Models/Stage1Map/**`
  - `Models/Stage2Map/**`
  - `Models/Stage1Map/Textures/**`

### P09_Modular_Humanoid

- Asset Store URL: https://assetstore.unity.com/packages/3d/characters/humanoids/fantasy/p09-modular-humanoid-305379
- Publisher: Sabao3179
- License agreement: Standard Unity Asset Store EULA
- Store page license type shown: Single Entity / Multi Entity
- Commercial game distribution: Allowed as embedded content in a licensed product,
  subject to the EULA.
- Attribution required: Not generally required by the Standard Unity Asset Store
  EULA.
- Used/derived files:
  - `Models/Player/*.fbx`
  - `Models/Weapons/*.fbx`
  - `Textures/P09_*.dds`

Note: the store page lists `Magica Cloth 2` as a package dependency. If any
runtime files, scripts, or generated data from Magica Cloth 2 are included in the
shipping build, record that license separately. If it was only used during Unity
asset preparation and no files are shipped, keep that workflow note internally.

## Ready-to-Credit Assets

### Sketchfab - medieval village strategies pack

- Asset: `medieval village strategies pack`
- Author: Gnossiennes
- Source: https://skfb.ly/pqzKS
- License: Creative Commons Attribution 4.0 International (CC BY 4.0)
- License URL: https://creativecommons.org/licenses/by/4.0/
- Commercial use: Allowed, with attribution.
- Attribution required: Yes.
- Repository evidence: `ThirdPartyAssetCredits.txt`
- Used/derived files:
  - `Models/Village/village.fbx`
  - `Models/Village/VillageFloorCollider.fbx`
  - `Models/Village/VillageWallCollider.fbx`
  - `Textures/village_textures/*.dds`

Suggested credit text:

```text
"medieval village strategies pack" (https://skfb.ly/pqzKS) by Gnossiennes
is licensed under Creative Commons Attribution 4.0
(https://creativecommons.org/licenses/by/4.0/).
```

### OpenGameArt - Archer buff aura textures

- License: Creative Commons Zero (CC0)
- License URL: https://creativecommons.org/publicdomain/zero/1.0/
- Commercial use: Allowed.
- Attribution required: No.
- Repository evidence: `Textures/Effect/OpenGameArt_ArcherBuff_LICENSE.txt`

Files:

| File | Source | Author | URL | License |
| --- | --- | --- | --- | --- |
| `Textures/Effect/archer_buff_circle4.dds` | `4 summoning circles` | Luke.RUSTLTD | https://opengameart.org/content/4-summoning-circles | CC0 |
| `Textures/Effect/archer_buff_whirl2.dds` | `Whirlwind for effects` | n4 | https://opengameart.org/content/whirlwind-for-effects | CC0 |
| `Textures/Effect/archer_wind_trail_00.dds` | `Trail` | p0ss | https://opengameart.org/content/trail | CC0 |

Optional credit text:

```text
Some archer buff effect textures use CC0 assets from OpenGameArt:
"4 summoning circles" by Luke.RUSTLTD, "Whirlwind for effects" by n4,
and "Trail" by p0ss.
```

### Kenney - Particle Pack

- Asset: `Particle Pack`
- Author: Kenney Vleugels (Kenney.nl)
- Source: https://kenney.nl/
- License: Creative Commons Zero (CC0)
- License URL: https://creativecommons.org/publicdomain/zero/1.0/
- Commercial use: Allowed.
- Attribution required: No.
- Repository evidence: `Textures/Effect/KenneyParticlePack_LICENSE.txt`
- Used/derived files: `Textures/Effect/*.dds` that originated from Kenney Particle Pack.

Important: the repository does not currently map each `Textures/Effect/*.dds`
file to its exact source. Keep the original Kenney license file and verify any
effect texture that did not come from Kenney or the OpenGameArt entries above.

Optional credit text:

```text
Particle effects include assets from Kenney Particle Pack by Kenney Vleugels
(Kenney.nl), released under CC0.
```

## Conditionally Usable, Needs Source Proof

### Adobe Mixamo characters and animations

- License summary from Adobe Mixamo FAQ: Mixamo characters and animations may be
  used royalty-free in personal, commercial, and non-profit projects.
- Official FAQ: https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html
- Commercial use: Generally allowed under Mixamo terms.
- Attribution required: Not generally required by the FAQ.
- Repository evidence: many animation names and skeleton socket names look like
  Mixamo-style assets, but no local source manifest identifies which files came
  from Mixamo.

Potentially related files that need source confirmation:

- `Models/Animated/Dash.fbx`
- `Models/Animated/Female_Warrior/*.fbx`
- `Models/Animated/Male_Archer/*.fbx`
- `Models/Animated/Male_Wizard/*.fbx`
- `Models/Animated/ShopKeeper/*.fbx`

Before Steam release, confirm each file's source and keep the download/source
records. If any file was not from Mixamo, record its real license separately.

### NASA eclipse/sky imagery

- NASA media usage guidelines: https://www.nasa.gov/nasa-brand-center/images-and-media/
- Commercial use: Often allowed for NASA-created media, but do not imply NASA
  endorsement and do not use NASA logos/insignia as branding.
- Attribution: NASA should be acknowledged as source when NASA imagery is used.
- Repository evidence: filename only.

Files needing exact source URL:

- `Textures/Sky/eclipse_nasa_grc_2024.dds`

Suggested placeholder credit after confirming source:

```text
Eclipse imagery courtesy of NASA. Use does not imply NASA endorsement.
```

Do not ship this as final attribution until the exact NASA source page/caption is
recorded.

### Pixabay audio assets

- Source: https://pixabay.com/music/ and https://pixabay.com/sound-effects/
- License/terms: Pixabay Content License, or CC0 for Pixabay content published
  before January 9, 2019.
- License URL: https://pixabay.com/service/terms/
- License summary URL: https://pixabay.com/service/license-summary/
- Commercial use: Allowed, subject to Pixabay's prohibited uses.
- Attribution required: No.
- Repository evidence: project owner stated the sound files were selected from a
  previously recommended free sound source. The local MP3 files do not currently
  preserve exact source page metadata.

Potentially related files:

- `Sounds/**/*.mp3`

Steam release requirement:

Keep the exact Pixabay download page URL, contributor name, download date, and a
license screenshot or downloaded license record for every music/SFX file. This is
important because the repository currently contains renamed MP3 files, so the
original Pixabay item pages cannot be proven from the files alone.

Suggested optional credit text:

```text
Some music and sound effects are from Pixabay, used under the Pixabay Content
License. Attribution is not required by Pixabay, but contributor records are
kept internally.
```

## NEEDS VERIFICATION Before Steam Release

The following asset groups are present in the repository or referenced by game
code, but no usable license/source proof was found in the repo or current task
history. Do not assume these are safe for Steam distribution.

### Player, armor, and weapon models/textures

Resolved source: `P09_Modular_Humanoid` from Unity Asset Store. Keep proof of
purchase and verify whether the project needs Single Entity or Multi Entity.

### Skeleton, demon, imp, and boss models/animations/textures

- `Models/Imp/Animation/Imp/**`
- `Models/Imp/Model/SKM_Demon.fbx`
- `Textures/T_Demon_Base_color.dds`
- `Textures/T_Demon_Base_color.tga`

Resolved sources:

- `Models/Skeleton2/**` and `Textures/Archer Skeleton Classic.*`: Stylized Archer Skeleton, Unity Asset Store.
- `Models/Skeleton/**` and `Textures/Warrior Skeleton Classic.*`: Stylized Warrior Skeleton, Unity Asset Store.
- `Models/Boss/**`, `Models/Animated/Boss/**`, and `Textures/T_DemonLordBody_*`: DEMON LORD, Unity Asset Store.
- `Models/Imp/Animation/Imp_Archer/**`, `Models/Imp/Model/SKM_Imp_Archer.fbx`, and `Textures/T_Demon_Archer_Base_color.*`: Demon Archer - Game ready character, Unity Asset Store.

Remaining action: confirm the source for the non-archer demon files listed above.

### Stage maps and sky textures other than the village

Resolved source:

- `Models/Stage1Map/**`, `Models/Stage2Map/**`, and `Models/Stage1Map/Textures/**`: Stylized dungeon, Unity Asset Store.

Still needs verification:

- `Textures/sky.dds`
- `Textures/sky_stage1.dds`
- `Textures/sky_stage2.dds`
- `Textures/sky_village.dds`
- `Textures/Sky/FX_CloudAlpha05.dds`
- `Textures/Sky/FX_CloudAlpha08.dds`
- `Textures/Sky/moon_disc_overlay.dds`
- `Textures/Sky/sun_disc_overlay.dds`

Required action: record source URLs/licenses for each sky texture.

### Sound effects, music, and cutscene video

- `CutScene/BossCutScene.mp4`

Sound/music status:

- `Sounds/**/*.mp3` are documented above as likely Pixabay audio assets based on
  the project owner's current statement.

Remaining required action: attach or record the exact Pixabay item page URL,
contributor name, and download/license proof for each sound file. Also record the
source, author, and license for `CutScene/BossCutScene.mp4`. This remains high
priority because music/SFX/video licenses are often stricter than model/texture
licenses.

### UI textures, icons, and fonts

- `Textures/UI/**`
- `Textures/MainMenu.dds`
- `Textures/Title.dds`
- `Textures/chat_korean.spritefont`
- `Textures/myfile.spritefont`

Required action: identify the original images/icons/fonts used to generate these
files. Confirm commercial embedding/distribution rights for the fonts.

### Remaining effect textures

- `Textures/Effect/*.dds` not specifically listed under OpenGameArt above and
  not verified as part of Kenney Particle Pack.

Required action: map each file to OpenGameArt, Kenney, original work, or another
source license.

## Steam Release Checklist

Before uploading a public Steam build:

1. Keep this file in the repository and include required credits in-game or in a
   shipped `credits`/`licenses` file.
2. For every `NEEDS VERIFICATION` group, add source URL, author, license, and
   whether commercial game distribution is allowed.
3. Remove or replace any asset with `personal use only`, `non-commercial`,
   `editorial only`, unclear AI-generation restrictions, or no recoverable
   source proof.
4. Keep screenshots/PDFs/text exports of license pages for paid/free store
   assets, because store pages can change.
5. Do not imply endorsement by NASA, Adobe, Sketchfab, OpenGameArt, Kenney, or
   any third-party author.
