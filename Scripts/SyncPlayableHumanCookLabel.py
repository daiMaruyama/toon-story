"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file> after changing HumanAppearances."""
import unreal
from pathlib import Path


def sync():
    # Read the project's roster, not per-user Saved/Config overrides. ConfigParser
    # would collapse the repeated +HumanAppearances entries into a single value.
    paths = []
    section = ""
    for line in (Path(unreal.Paths.project_config_dir()) / "DefaultGame.ini").read_text(encoding="utf-8-sig").splitlines():
        line = line.strip()
        if line.startswith("["):
            section = line
        if section != "[/Script/ToonStory.TBDataSettings]" or "=" not in line:
            continue
        key, value = (field.strip() for field in line.split("=", 1))
        if key in ("!HumanAppearances", "HumanAppearances"):
            paths.clear()
        if key in ("+HumanAppearances", ".HumanAppearances", "HumanAppearances"):
            paths.append(value.strip('"'))
        elif key == "-HumanAppearances":
            paths = [p for p in paths if p != value.strip('"')]
    roster = [unreal.load_class(None, path) for path in dict.fromkeys(paths)]
    if not roster or any(not look for look in roster):
        raise RuntimeError("A configured HumanAppearances class could not be resolved")
    folder, name = "/Game/CookLabels", "PAL_PlayableHumans"
    asset_path = folder + "/" + name
    library = unreal.EditorAssetLibrary
    label = library.load_asset(asset_path) if library.does_asset_exist(asset_path) else None
    if label is None:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.PrimaryAssetLabel)
        label = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.PrimaryAssetLabel, factory)
    rules = label.get_editor_property("rules")
    if (rules.get_editor_property("cook_rule") == unreal.PrimaryAssetCookRule.ALWAYS_COOK
            and list(label.get_editor_property("explicit_blueprints")) == roster):
        unreal.log("Playable human cook label is already up to date.")
        return
    rules.set_editor_property("cook_rule", unreal.PrimaryAssetCookRule.ALWAYS_COOK)
    label.set_editor_property("rules", rules)
    label.set_editor_property("explicit_blueprints", list(roster))
    if not unreal.EditorAssetLibrary.save_loaded_asset(label, False):
        raise RuntimeError("Could not save " + label.get_path_name())
    unreal.log("Cook label: " + ", ".join(look.get_name() for look in roster))


sync()
