"""One-time creation of the Title asset. Never modifies an existing map."""
import unreal

path = "/Game/Maps/Title"
if unreal.EditorAssetLibrary.does_asset_exist(path):
    raise RuntimeError("Title already exists; edit it in Unreal instead of regenerating it.")
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not levels.new_level(path):
    raise RuntimeError("Could not create Title")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
mode = unreal.load_class(None, "/Script/ToonStory.TBTitleGameMode")
if not mode:
    raise RuntimeError("Build the Title classes before creating the map")
world.get_world_settings().set_editor_property("default_game_mode", mode)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 0, 200))
camera.set_actor_label("Title_BackgroundCamera")
if not levels.save_current_level():
    raise RuntimeError("Could not save Title")
unreal.log("TITLE_MAP_CREATED")
