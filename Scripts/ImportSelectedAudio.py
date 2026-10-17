"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
TOON_AUDIO_SOURCE can override the extracted candidate directory. Never imports pending assets.
"""
import hashlib
import json
import os
from pathlib import Path
import unreal

project = Path(unreal.Paths.project_dir()).resolve()
source = Path(os.environ.get("TOON_AUDIO_SOURCE", str(
    project.parent / "ToonStory_Audio_Candidates_20261012" / "ToonStory_Audio_Candidates")))
entries = json.loads((project / "docs/AUDIO_SELECTION.json").read_text(encoding="utf-8"))
for entry in entries:
    filename = source / entry["file"]
    if hashlib.sha256(filename.read_bytes()).hexdigest() != entry["sha256"]:
        raise RuntimeError("Source checksum mismatch: " + str(filename))

for entry in entries:
    path = entry["asset"]
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source / entry["file"]))
        task.set_editor_property("destination_path", path.rsplit("/", 1)[0])
        task.set_editor_property("destination_name", entry["id"])
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("factory", unreal.SoundFactory())
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sound = unreal.load_asset(path)
    if not isinstance(sound, unreal.SoundWave):
        raise RuntimeError("Missing imported SoundWave: " + path)
    sound.set_editor_property("looping", bool(entry["loop"] or entry["category"] == "BGM"))
    if not unreal.EditorAssetLibrary.save_loaded_asset(sound):
        raise RuntimeError("Could not save: " + path)
    if sound.get_editor_property("duration") <= 0:
        raise RuntimeError("Empty imported sound: " + path)
unreal.log("SELECTED_AUDIO_IMPORT_OK count=" + str(len(entries)))
