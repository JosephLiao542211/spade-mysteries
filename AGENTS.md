# Spade Mystery: agent notes

A deliberately tiny noir detective game (UE 5.8, C++). The only content assets are the owner's MetaHuman suspects.
You sit across a table from a suspect in a dark room and question them, then accuse one.

## Working rules (from the project owner)

- **Look and adjust yourself.** The owner asked the agent to verify visuals itself instead of asking them, which replaces the earlier no-windows rule. Use the `NoirSuspectToolset` in the owner's open editor:
  - `play(true/false)`;
  - `show_suspect(i)`;
  - `set_look(json, save)` changes any Noir Look value live;
  - `capture_frame(name)` saves the real 1-bit frame to `Saved/NoirShots/<name>.png`, which you then Read.

  Iterate until it looks right before reporting. Don't spam the owner with questions you can answer by looking.
- **Restarts:** the agent may quit and relaunch the editor itself (`save_suspects`, then `quit_editor`, then `open -a UnrealEditor.app --args <uproject>`). Never use Live Coding for UPROPERTY or USTRUCT changes; it scrambles values.
- **Keep the owner in the loop** with short status updates.
- **Realistic motion comes from real motion data** (Rokoko Vision mocap, retargeted with `import_mocap`), not procedural bone posing. The owner called the procedural pose broken.
- **Keep it minimal.** No extra systems unless asked.
- **Strict aesthetic:** a 3D scene rendered as **pure black and pure white only** (1-bit), with pixelation, halftone/dither patterns and harsh contrast. No grays, no color.

## Architecture (Source/Spade_mystery)

| File | Role |
|---|---|
| `NoirGameMode` | Sets the controller and HUD; no pawn. Default game mode for every map (`Config/DefaultEngine.ini`). |
| `NoirPlayerController` | Case data (suspects, questions, answers, culprit), game state, and key bindings (legacy `BindKey`). Spawns `ANoirRoom` at z=-50000. |
| `NoirRoom` | The 3D room, built at runtime from `/Engine/BasicShapes`: table, chair, a swaying lamp spotlight, a streetlight through venetian blinds, volumetric haze, and suspect figures. Owns a low-res HDR `SceneCaptureComponent2D`. Reads lighting, haze and camera values from the settings every tick. |
| `NoirHUD` | 1-bit compositor. Each frame: capture the scene, draw the UI into a mask target (red = white ink, green = black box), read both back, apply exposure, gamma, contrast and the dither screen, upload to a nearest-filtered texture, and draw it integer-scaled. |
| `NoirLookSettings` | **All tunables as sliders.** Project Settings → Game → **Noir Look**. Applies live during Play and saves to `Config/DefaultGame.ini`. |

Main viewport world rendering is disabled; the screen shows only the HUD's composited texture.

## Owner's tuning workflow

1. Press Play in the editor.
2. Open **Edit → Project Settings → Game → Noir Look** in a window beside the viewport.
3. Drag sliders; the game updates live:
   - **Pixels:** vertical resolution (chunkiness).
   - **Dither:** pattern (halftone dots, lines, Bayer 4/8, hard threshold), cell size, screen angle, contrast, pivot, gamma, invert.
   - **Exposure:** bias, white percentile, adapt speed.
   - **Lamp:** intensity, cone angle, softness, sway, flicker.
   - **Window:** streetlight intensity and cone.
   - **Haze:** density, beam strength.
   - **Rim Light:** backlight intensity and cone, which outlines the suspect.
   - **Camera:** FOV, distance, height, aim height, drift.
   - **Suspects:** one slot per suspect: character Blueprint, idle animation, offset, yaw, scale. There is also a "force highest detail" toggle.
4. Values save to `Config/DefaultGame.ini` automatically.

## Suspect characters (MetaHuman)

- The `MetaHumanCharacter` plugin (MetaHuman Creator) is enabled for the editor only.
- The owner creates MetaHumans in the editor and assembles them to Blueprints. They then assign each one to a slot in Noir Look → Suspects.
- `ANoirRoom::SyncSuspectCharacters` spawns, re-spawns or places them live. The idle animation plays on the component named `Body`. An empty slot falls back to the primitive-shape figure.
- The `MetaHumanGenerator` plugin (Epic's experimental AI toolset) is enabled. Through the Unreal MCP connection to the owner's open editor, it lets the agent create MetaHuman Character assets and set body shape, skin tone and eye color. Faces, hair, clothes and Assemble remain the owner's job.
- `Content/Python/noir_suspects.py` (registered by `Content/Python/init_unreal.py` at editor start) is the project's own MCP toolset `NoirSuspectToolset`:
  - `status` reports whether Core Data is installed and which suspects are built.
  - `build_suspect` sets body, face variation, skin, eyes, optionally Assembles, and fills the slot.
  - `assign_built_suspects` fills slots from finished builds.
  - Characters live in `/Game/Suspects/<Name>/`.
- Realistic skin needs **MetaHuman Creator Core Data**, an optional engine component installed from the Epic Games Launcher (it puts an `Optional` folder in the MetaHumanCharacter plugin). Without it, MetaHumans look gray and plastic. Only the owner can install it.
- Driving the owner's already-open editor through MCP is allowed. Launching new windows is not.
- Suspects are copies of Epic presets (`/MetaHumanCharacter/Optional/Presets`): Vera = Vivian, Mickey = Bruce, Graves = Dominic. Assembled Blueprints live at `/Game/Suspects/<Name>/MHC_<Name>/BP_MHC_<Name>`.
- **Mocap pipeline:** Rokoko Vision FBX → `import_mocap(fbx_path, clip_name, index)`. It imports to `/Game/Mocap/<clip>`, auto-builds IK rigs and a retargeter onto the suspect's MetaHuman body, retargets, sets the slot's Idle Animation, and saves.
- Cloth simulation is disabled on spawned suspects; it exploded when they were moved. Auto-centering only runs for 45 frames after spawn or a clip change.
- Never write raw face-model coefficients (`set_face_model_coefficients`). It corrupted a character and crashed the editor.
- A MetaHuman faces the camera at slot Yaw = 90.
- Default performance: when a slot has no animations, `ANoirRoom::DefaultPerformance` plays paired body and face Expression_Loops. Vera is sad, Mickey fearful, Graves angry.
- The set is life-size: a 70×110 cm table (Table Distance slider), a chair that only shows with the placeholder figure, and a mic stand for Vera (Mic Distance and Mic Height sliders).
- **Seated performance (experimental, off by default):** `UNoirPoseAnimInstance` is a C++ anim instance with a custom proxy `Evaluate`. It is set on the MetaHuman `Body` when the slot's **Seated** is on (the default). It does:
  - exaggerated proportions, by stretching bone translations and head scale;
  - hips dropped into the chair;
  - a slow spine lean, with the neck countering it;
  - breathing and a head tilt;
  - two-bone IK for both arms, with the left elbow on the desk and the right hand moving between rest and her lips on a smoke cycle.

  Hand targets are in her own Forward/Right/Up space, worked out from the skeleton. Every value is a slider in the slot's **Pose**. A cigarette holder (dark stick plus white tip) is attached to the `hand_r` bone and owned by the suspect actor.
- The camera is framed on the upper body. The desk has a solid front panel to hide legs.
- MetaHumans stand when Seated is off. A sitting animation is the owner's choice; otherwise they stand behind the table.

## Known risks / open questions

- CPU readback (`ReadPixels`) stalls the GPU each frame. It's fine at low resolution, but ask the owner about FPS. If it's too slow, move dithering into a post-process material or a global shader.
- The lamp and streetlight intensities are untested against the auto exposure. The owner tunes them.
- MetaHumans are heavy: first load and shader compile take time, and grooms cost GPU. They are not yet verified in the low-res capture.
