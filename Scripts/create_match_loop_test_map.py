"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.

Creates new, isolated test assets. Refuses to overwrite an existing destination.
"""
import unreal

BP_PATH = "/Game/Tests/MatchLoop/BP_MatchLoopGameMode"
MAP_PATH = "/Game/Tests/MatchLoop/Lvl_MatchLoop"
assets = unreal.EditorAssetLibrary
if assets.does_asset_exist(BP_PATH) or assets.does_asset_exist(MAP_PATH):
    raise RuntimeError("MatchLoop test assets already exist; refusing to overwrite them.")

assets.make_directory("/Game/Tests/MatchLoop")
blueprint = assets.duplicate_asset(
    "/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode", BP_PATH
)
if not blueprint:
    raise RuntimeError("Could not duplicate the existing game mode.")
parent = unreal.load_class(None, "/Script/ToonStory.BatteryTagGameMode")
unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, parent)
if not unreal.BlueprintEditorLibrary.compile_blueprint(blueprint):
    raise RuntimeError("Test GameMode Blueprint did not compile.")
defaults = unreal.get_default_object(assets.load_blueprint_class(BP_PATH))
defaults.set_editor_property("game_state_class", unreal.load_class(None, "/Script/ToonStory.BatteryTagGameState"))
defaults.set_editor_property("player_state_class", unreal.load_class(None, "/Script/ToonStory.ToonStoryPlayerState"))
defaults.set_editor_property("hud_class", unreal.load_class(None, "/Script/ToonStory.ToonMatchHUD"))
defaults.set_editor_property("enable_match_loop", True)
defaults.set_editor_property("round_duration_seconds", 30.0)
defaults.set_editor_property("start_countdown_seconds", 3.0)
defaults.set_editor_property("result_display_seconds", 8.0)
defaults.set_editor_property("minimum_players", 3)
unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
if not assets.save_loaded_asset(blueprint, only_if_is_dirty=False):
    raise RuntimeError("Could not save the test GameMode.")

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not levels.new_level(MAP_PATH):
    raise RuntimeError("Could not create the test map.")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", assets.load_blueprint_class(BP_PATH))
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -50))
floor.set_actor_label("MatchLoop Floor")
floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
floor.set_actor_scale3d(unreal.Vector(30, 30, 1))
for index, x in enumerate((-300, 0, 300)):
    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(x, 0, 100))
    start.set_actor_label("PlayerStart_" + str(index + 1))
sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(-60, 0, 0))
sun.set_actor_label("MatchLoop Sun")
actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 400))
if not levels.save_current_level():
    raise RuntimeError("Could not save the test map.")
unreal.log("MATCH_LOOP_ASSETS_CREATED: " + MAP_PATH)
