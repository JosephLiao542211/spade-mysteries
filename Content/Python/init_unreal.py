"""Runs when the editor starts. Registers the Spade Mystery suspect toolset for the AI (Unreal MCP) connection.
Can be re-run any time from the editor console with:  py init_unreal.py   (not during Play)
"""

import importlib
import sys

import unreal

if unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem) is None:
    unreal.log_warning("[Noir] MetaHuman Creator not available; suspect toolset not registered")
else:
    old = sys.modules.get("noir_suspects")
    if old is not None:
        try:
            unreal.ToolsetRegistry.unregister_toolset_class(old.NoirSuspectToolset)
        except Exception as error:  # stale registration; carry on
            unreal.log_warning(f"[Noir] unregister: {error}")
        noir_suspects = importlib.reload(old)
    else:
        import noir_suspects

    unreal.ToolsetRegistry.register_toolset_class(noir_suspects.NoirSuspectToolset)
    unreal.log("[Noir] Suspect toolset registered")
