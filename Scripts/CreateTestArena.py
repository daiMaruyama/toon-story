"""UEエディタのPythonコマンドレットで、初回の試遊マップを作成する。"""
import os
import unreal

MAP_PATH = "/Game/Maps/Arena"
map_file = os.path.join(unreal.Paths.project_content_dir(), "Maps", "Arena.umap")
if os.path.exists(map_file):
    raise RuntimeError("Arena already exists; refusing to overwrite it.")

world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
game_mode = unreal.load_class(None, "/Script/ToonStory.TBGameMode")
if not world or not game_mode:
    raise RuntimeError("Could not create world or load TBGameMode.")
world.get_world_settings().set_editor_property("default_game_mode", game_mode)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# 床・壁・プレイヤー・箱・アイテムはゲーム開始時にC++が生成する。
sun = actors.spawn_actor_from_class(
    unreal.DirectionalLight, unreal.Vector(0, 0, 1500), unreal.Rotator(-50, -30, 0)
)
sun.set_actor_label("Arena Sun")
sun_component = sun.get_component_by_class(unreal.DirectionalLightComponent)
sun_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sun_component.set_intensity(3.0)

sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1000))
sky.set_actor_label("Arena Ambient Light")
sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky_component.set_intensity(1.0)
sky_component.set_editor_property("real_time_capture", True)

atmosphere = actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
atmosphere.set_actor_label("Arena Sky")
sun_component.set_editor_property("atmosphere_sun_light", True)

if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
    raise RuntimeError("Could not save Arena.")
unreal.log("ARENA_CREATED: " + MAP_PATH)
