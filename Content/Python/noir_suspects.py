"""Spade Mystery: turn the three suspects into realistic MetaHumans and seat them in the game.

Each suspect starts as a copy of one of Epic's finished MetaHuman presets (real face, hair, clothes)
from the MetaHuman Creator Core Data, then gets auto-rigged and assembled into a Blueprint.

Registered as an AI toolset (see init_unreal.py) so the agent can drive it through the
Unreal MCP connection in the owner's already-open editor. Nothing here opens windows.
Never write raw face-model coefficients: that corrupted a character and crashed the editor once.
"""

from __future__ import annotations

import json

import unreal
import toolset_registry

_MH = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)

SUSPECTS_ROOT = "/Game/Suspects"
SUSPECT_NAMES = ["Vera", "Mickey", "Graves"]
PRESETS_ROOT = "/MetaHumanCharacter/Optional/Presets"


def _core_data_installed() -> bool:
    wrapper = getattr(unreal, "MetaHumanGeneratorSubsystemWrapper", None)
    return bool(wrapper and wrapper.is_optional_content_installed())


def _character_path(name: str) -> str:
    return f"{SUSPECTS_ROOT}/{name}/MHC_{name}"


def _require_not_playing() -> None:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les and les.is_in_play_in_editor():
        raise RuntimeError("The game is running (Play mode). Stop Play in the editor, then try again.")


def _check_index(index: int) -> str:
    if index < 0 or index >= len(SUSPECT_NAMES):
        raise ValueError("index must be 0 (Vera), 1 (Mickey) or 2 (Graves)")
    return SUSPECT_NAMES[index]


def _load_existing(index: int):
    _require_not_playing()
    name = _check_index(index)
    character = unreal.load_asset(_character_path(name))  # direct load; the registry may still be scanning
    if not character:
        raise ValueError(f"{name} has not been created yet; call create_from_preset first")
    return name, character


def _game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def _look():
    return unreal.get_default_object(unreal.NoirLookSettings)


def _get_path(obj, path: str):
    for part in path.split("."):
        obj = obj.get_editor_property(part)
    return obj


def _set_path(root, path: str, value):
    """Set a dotted property path on the settings CDO; structs are copied, edited, and written back."""
    parts = path.split(".")
    chain = [root]
    for part in parts[:-1]:
        chain.append(chain[-1].get_editor_property(part))
    target = chain[-1]
    current = target.get_editor_property(parts[-1])
    if isinstance(current, unreal.Vector) and isinstance(value, (list, tuple)):
        value = unreal.Vector(*value)
    elif isinstance(current, unreal.Rotator) and isinstance(value, (list, tuple)):
        value = unreal.Rotator(roll=value[2], pitch=value[0], yaw=value[1])
    target.set_editor_property(parts[-1], value)
    for i in range(len(parts) - 2, -1, -1):
        chain[i].set_editor_property(parts[i], chain[i + 1])


def _to_json(value):
    if isinstance(value, unreal.Vector):
        return [value.x, value.y, value.z]
    if isinstance(value, unreal.Rotator):
        return [value.pitch, value.yaw, value.roll]
    if isinstance(value, unreal.StructBase):
        out = {}
        for name in dir(value):
            if name.startswith("_"):
                continue
            try:
                v = value.get_editor_property(name)
            except Exception:
                continue
            out[name] = _to_json(v)
        return out
    if isinstance(value, (bool, int, float, str)) or value is None:
        return value
    return str(value)


class _Editing:
    """Keeps a character in MetaHuman edit mode for the duration of a block."""

    def __init__(self, character):
        self.character = character
        self.added = False

    def __enter__(self):
        if not _MH.is_object_added_for_editing(self.character):
            if not _MH.try_add_object_to_edit(character=self.character):
                raise RuntimeError("Could not enter MetaHuman edit mode")
            self.added = True
        return self.character

    def __exit__(self, *exc):
        if self.added:
            _MH.remove_object_to_edit(self.character)
        return False


def _find_blueprints(name: str) -> list[str]:
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = []
    for data in registry.get_assets_by_path(f"{SUSPECTS_ROOT}/{name}", recursive=True):
        if str(data.asset_class_path.asset_name) == "Blueprint":
            found.append(str(data.package_name) + "." + str(data.asset_name))
    return found


def _assign_slot(index: int, name: str) -> str | None:
    blueprints = _find_blueprints(name)
    if not blueprints:
        return None
    blueprints.sort(key=lambda p: (not p.split(".")[-1].startswith("BP_"), len(p)))
    bp = blueprints[0]
    cls = unreal.EditorAssetLibrary.load_blueprint_class(bp)
    if not cls:
        return None
    unreal.NoirLookSettings.set_suspect_character(index, cls)
    return bp


@unreal.uclass()
class NoirSuspectToolset(unreal.ToolsetDefinition):
    """Spade Mystery: make the three suspects realistic MetaHumans (from Epic presets), rig, assemble, and seat them in the game."""

    @toolset_registry.tool_call
    def status() -> str:
        """Report whether MetaHuman Creator Core Data is installed and which suspects exist / are assembled.

        Returns:
            JSON summary.
        """
        report = {"core_data_installed": _core_data_installed(), "suspects": {}}
        for name in SUSPECT_NAMES:
            report["suspects"][name] = {
                "character_asset": unreal.EditorAssetLibrary.does_asset_exist(_character_path(name)),
                "blueprints": _find_blueprints(name),
            }
        return json.dumps(report)

    @toolset_registry.tool_call
    def list_presets() -> str:
        """List Epic's finished MetaHuman presets that can be used as a starting point.

        Returns:
            JSON list of preset names.
        """
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        names = sorted(str(a.asset_name) for a in registry.get_assets_by_path(PRESETS_ROOT, recursive=False))
        return json.dumps(names)

    @toolset_registry.tool_call
    def create_from_preset(index: int, preset: str) -> str:
        """Make suspect `index` a copy of an Epic MetaHuman preset (face, hair, clothes included).
        Replaces any existing character for that suspect.

        Args:
            index: 0=Vera, 1=Mickey, 2=Graves.
            preset: Preset name from list_presets, e.g. "Vivian".

        Returns:
            JSON with what happened.
        """
        _require_not_playing()
        name = _check_index(index)
        source = f"{PRESETS_ROOT}/{preset}"
        if not unreal.EditorAssetLibrary.does_asset_exist(source):
            raise ValueError(f"No preset named {preset}; see list_presets")
        dest = _character_path(name)
        if unreal.EditorAssetLibrary.does_asset_exist(dest):
            unreal.EditorAssetLibrary.delete_asset(dest)
        copy = unreal.EditorAssetLibrary.duplicate_asset(source, dest)
        if not copy:
            raise RuntimeError(f"Could not copy {source} to {dest}")
        unreal.EditorAssetLibrary.save_loaded_asset(copy, only_if_is_dirty=False)
        return json.dumps({"suspect": name, "preset": preset, "asset": dest})

    @toolset_registry.tool_call
    def rig_suspect(index: int, full_face_rig: bool, high_res_textures: bool) -> str:
        """Run MetaHuman auto-rigging (required before assembling) and optionally fetch high-resolution skin textures.
        Both are Epic cloud services; this blocks until they finish and may require the owner to be signed in to Epic.

        Args:
            index: 0=Vera, 1=Mickey, 2=Graves.
            full_face_rig: True = joints + blend shapes (best facial expressions), False = joints only (faster).
            high_res_textures: Also request high-resolution synthesized skin textures.

        Returns:
            JSON with what happened.
        """
        name, character = _load_existing(index)
        result = {"suspect": name}
        with _Editing(character):
            rig = unreal.MetaHumanCharacterAutoRiggingRequestParams()
            rig.rig_type = unreal.MetaHumanRigType.JOINTS_AND_BLEND_SHAPES if full_face_rig else unreal.MetaHumanRigType.JOINTS_ONLY
            rig.blocking = True
            rig.report_progress = True
            _MH.request_auto_rigging(character, rig)
            result["rig"] = "requested"
            if high_res_textures:
                tex = unreal.MetaHumanCharacterTextureRequestParams()
                tex.blocking = True
                tex.report_progress = True
                _MH.request_texture_sources(character, tex)
                result["textures"] = "requested"
            unreal.EditorAssetLibrary.save_loaded_asset(character, only_if_is_dirty=False)
            result["can_assemble"] = bool(_MH.can_build_meta_human(character, False))
        return json.dumps(result)

    @toolset_registry.tool_call
    def assemble_suspect(index: int) -> str:
        """Assemble (build) suspect `index` into a Blueprint and put it in that suspect's Noir Look slot.

        Args:
            index: 0=Vera, 1=Mickey, 2=Graves.

        Returns:
            JSON with what happened.
        """
        name, character = _load_existing(index)
        with _Editing(character):
            if not _MH.can_build_meta_human(character, True):
                outcome = "not possible yet (see Output Log; usually needs rig_suspect first)"
            else:
                params = unreal.MetaHumanCharacterEditorBuildParameters()
                params.absolute_build_path = f"{SUSPECTS_ROOT}/{name}"
                _MH.build_meta_human(character, params)
                outcome = "done"
        bp = _assign_slot(index, name)
        return json.dumps({"suspect": name, "assemble": outcome, "slot_blueprint": bp})

    @toolset_registry.tool_call
    def assign_built_suspects() -> str:
        """Put any assembled suspect Blueprints into their Noir Look slots.

        Returns:
            JSON map of suspect -> Blueprint path (or null).
        """
        return json.dumps({name: _assign_slot(i, name) for i, name in enumerate(SUSPECT_NAMES)})

    @toolset_registry.tool_call
    def save_suspects() -> str:
        """Save every asset under /Game/Suspects (assembled MetaHumans are created in memory and must be saved).

        Returns:
            JSON with the number of assets on disk afterwards.
        """
        ok = unreal.EditorAssetLibrary.save_directory(SUSPECTS_ROOT, only_if_is_dirty=False, recursive=True)
        listed = unreal.EditorAssetLibrary.list_assets(SUSPECTS_ROOT, recursive=True, include_folder=False)
        return json.dumps({"saved": bool(ok), "assets": len(listed)})

    @toolset_registry.tool_call
    def inspect_suspect(index: int) -> str:
        """List the components of suspect `index`'s assembled Blueprint (to check hair/groom and mesh names).

        Args:
            index: 0=Vera, 1=Mickey, 2=Graves.

        Returns:
            JSON with component names and classes.
        """
        name = _check_index(index)
        bps = _find_blueprints(name)
        if not bps:
            return json.dumps({"suspect": name, "blueprint": None})
        cls = unreal.EditorAssetLibrary.load_blueprint_class(bps[0])
        cdo = unreal.get_default_object(cls)
        comps = []
        subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        handles = subsystem.k2_gather_subobject_data_for_blueprint(unreal.EditorAssetLibrary.load_asset(bps[0].split(".")[0]))
        for h in handles:
            data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)
            obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
            if obj:
                entry = {"name": obj.get_name(), "class": obj.get_class().get_name()}
                if isinstance(obj, unreal.GroomComponent):
                    entry["groom"] = str(obj.get_editor_property("groom_asset").get_path_name()) if obj.get_editor_property("groom_asset") else None
                comps.append(entry)
        return json.dumps({"suspect": name, "blueprint": bps[0], "components": comps})

    @toolset_registry.tool_call
    def play(start: bool) -> str:
        """Start or stop Play-In-Editor.

        Args:
            start: True to start playing, False to stop.

        Returns:
            Text status.
        """
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if start:
            if not les.is_in_play_in_editor():
                les.editor_request_begin_play()
            return "play requested"
        if les.is_in_play_in_editor():
            les.editor_request_end_play()
        return "stop requested"

    @toolset_registry.tool_call
    def show_suspect(index: int) -> str:
        """While playing: jump straight to interrogating suspect `index` (0=Vera, 1=Mickey, 2=Graves).

        Args:
            index: Suspect index.

        Returns:
            Text status.
        """
        world = _game_world()
        if not world:
            raise RuntimeError("Not playing; call play(true) first")
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        pc.debug_show_suspect(index)
        return f"showing suspect {index}"

    @toolset_registry.tool_call
    def capture_frame(name: str) -> str:
        """While playing: save the game's current 1-bit frame as a PNG in Saved/NoirShots.

        Args:
            name: File name without extension.

        Returns:
            Absolute path of the PNG.
        """
        world = _game_world()
        if not world:
            raise RuntimeError("Not playing; call play(true) first")
        hud = unreal.GameplayStatics.get_player_controller(world, 0).get_hud()
        path = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir() + f"NoirShots/{name}.png")
        if not hud.save_frame(path):
            raise RuntimeError("No frame yet")
        return path

    @toolset_registry.tool_call
    def get_look(path: str) -> str:
        """Read a Noir Look setting by dotted snake_case path, e.g. "suspect1.pose" or "camera_distance". Empty path = everything.

        Args:
            path: Dotted property path.

        Returns:
            JSON value.
        """
        look = _look()
        if not path:
            names = [n for n in dir(look) if not n.startswith("_")]
            out = {}
            for n in names:
                try:
                    out[n] = _to_json(look.get_editor_property(n))
                except Exception:
                    pass
            return json.dumps(out)
        return json.dumps(_to_json(_get_path(look, path)))

    @toolset_registry.tool_call
    def set_look(changes_json: str, save: bool) -> str:
        """Change Noir Look settings live (applies immediately during Play).

        Args:
            changes_json: JSON object of dotted snake_case path -> value, e.g.
                {"camera_distance": 200, "suspect1.pose.seat_drop": 40, "suspect1.pose.left_hand": [10, -5, -15]}.
                Vectors are [x, y, z]; rotators are [pitch, yaw, roll].
            save: Also write the values to Config/DefaultGame.ini.

        Returns:
            JSON of the new values.
        """
        look = _look()
        changes = json.loads(changes_json)
        for path, value in changes.items():
            _set_path(look, path, value)
        if save:
            unreal.NoirLookSettings.save_noir_look()
        return json.dumps({p: _to_json(_get_path(look, p)) for p in changes})

    @toolset_registry.tool_call
    def import_mocap(fbx_path: str, clip_name: str, index: int) -> str:
        """Import a motion-capture FBX (e.g. from Rokoko Vision), retarget it onto suspect `index`'s MetaHuman body,
        and set it as that suspect's looping Idle Animation (saved to config).

        Args:
            fbx_path: Absolute path of the .fbx on this Mac.
            clip_name: Short name for the clip, e.g. "VeraSmoke".
            index: 0=Vera, 1=Mickey, 2=Graves.

        Returns:
            JSON describing every step.
        """
        _require_not_playing()
        name = _check_index(index)
        steps = {}
        folder = f"/Game/Mocap/{clip_name}"
        assets = unreal.AssetToolsHelpers.get_asset_tools()
        lib = unreal.EditorAssetLibrary
        registry = unreal.AssetRegistryHelpers.get_asset_registry()

        # 1. Import the FBX (skeletal mesh + skeleton + animation).
        task = unreal.AssetImportTask()
        task.filename = fbx_path
        task.destination_path = folder
        task.automated = True
        task.replace_existing = True
        task.save = True
        options = unreal.FbxImportUI()
        options.import_mesh = True
        options.import_as_skeletal = True
        options.import_animations = True
        options.import_materials = False
        options.import_textures = False
        options.mesh_type_to_import = unreal.FBXImportType.FIT_SKELETAL_MESH
        task.options = options
        assets.import_asset_tasks([task])
        found = registry.get_assets_by_path(folder, recursive=True)
        def first(cls_name):
            for a in found:
                if str(a.asset_class_path.asset_name) == cls_name:
                    return lib.load_asset(str(a.package_name))
            return None
        src_mesh = first("SkeletalMesh")
        src_anim = first("AnimSequence")
        steps["imported"] = [str(a.asset_name) + ":" + str(a.asset_class_path.asset_name) for a in found]
        if not src_mesh or not src_anim:
            raise RuntimeError(f"Import did not produce a skeletal mesh + animation: {steps['imported']}")

        # 2. The suspect's MetaHuman body mesh.
        tgt_mesh = None
        for a in registry.get_assets_by_path(f"{SUSPECTS_ROOT}/{name}", recursive=True):
            if str(a.asset_class_path.asset_name) == "SkeletalMesh" and "body" in str(a.asset_name).lower():
                tgt_mesh = lib.load_asset(str(a.package_name))
                break
        if not tgt_mesh:
            raise RuntimeError(f"No assembled body mesh found for {name}")
        steps["target_mesh"] = tgt_mesh.get_path_name()

        # 3. IK rigs (auto-generated chains + full-body IK) for both skeletons.
        def make_rig(asset_name, mesh):
            path = f"{folder}/{asset_name}"
            if lib.does_asset_exist(path):
                lib.delete_asset(path)
            rig = assets.create_asset(asset_name, folder, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
            ctrl = unreal.IKRigController.get_controller(rig)
            ctrl.set_skeletal_mesh(mesh)
            ctrl.apply_auto_generated_retarget_definition()
            ctrl.apply_auto_fbik()
            return rig
        src_rig = make_rig(f"IK_{clip_name}_Source", src_mesh)
        tgt_rig = make_rig(f"IK_{clip_name}_{name}", tgt_mesh)

        # 4. Retargeter: map chains automatically and align the rest poses (T-pose vs A-pose).
        rtg_name = f"RTG_{clip_name}_{name}"
        if lib.does_asset_exist(f"{folder}/{rtg_name}"):
            lib.delete_asset(f"{folder}/{rtg_name}")
        rtg = assets.create_asset(rtg_name, folder, unreal.IKRetargeter, unreal.IKRetargetFactory())
        rc = unreal.IKRetargeterController.get_controller(rtg)
        rc.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, src_rig)
        rc.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, tgt_rig)
        rc.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, src_mesh)
        rc.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, tgt_mesh)
        rc.add_default_ops()
        rc.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, src_rig)
        rc.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, tgt_rig)
        rc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
        rc.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)

        # 5. Retarget the clip.
        inputs = unreal.IKRetargetBatchOperationInputs()
        inputs.assets_to_retarget = [registry.get_asset_by_object_path(src_anim.get_path_name())]
        inputs.source_mesh = src_mesh
        inputs.target_mesh = tgt_mesh
        inputs.ik_retarget_asset = rtg
        inputs.target_path = f"{folder}/{name}"
        inputs.prefix = f"{name}_"
        inputs.overwrite_existing_files = True
        inputs.include_referenced_assets = False
        out = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
        retargeted = None
        for a in out:
            if str(a.asset_class_path.asset_name) == "AnimSequence":
                retargeted = lib.load_asset(str(a.package_name))
        if not retargeted:
            raise RuntimeError("Retarget produced no animation")
        lib.save_directory(folder, only_if_is_dirty=False, recursive=True)
        steps["animation"] = retargeted.get_path_name()

        # 6. Play it on the suspect (mocap replaces the procedural pose) and save.
        look = _look()
        slot_name = f"suspect{index + 1}"
        slot = look.get_editor_property(slot_name)
        slot.set_editor_property("idle_animation", retargeted)
        slot.set_editor_property("seated", False)
        look.set_editor_property(slot_name, slot)
        unreal.NoirLookSettings.save_noir_look()
        steps["slot"] = slot_name
        return json.dumps(steps)

    @toolset_registry.tool_call
    def quit_editor() -> str:
        """Quit the editor (after saving suspects) so new C++ code can load on the next launch.

        Returns:
            Text status.
        """
        unreal.SystemLibrary.quit_editor()
        return "quitting"

    @toolset_registry.tool_call
    def reload_toolset() -> str:
        """Reload this toolset from disk after the agent edits noir_suspects.py (no editor restart needed).

        Returns:
            Confirmation text.
        """
        import importlib
        import sys

        unreal.ToolsetRegistry.unregister_toolset_class(sys.modules[__name__].NoirSuspectToolset)
        module = importlib.reload(sys.modules[__name__])
        unreal.ToolsetRegistry.register_toolset_class(module.NoirSuspectToolset)
        return "reloaded"
