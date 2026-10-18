"""Generate the standalone lounge using existing ToyWorld assets.
Layout reference: IKEA, A traditional living room with space for everyone:
https://www.ikea.com/gb/en/rooms/living-room/gallery/a-traditional-living-room-with-space-for-everyone-pub63f26030/
Interpretation: grouped sofa + coffee table, wall storage, window reading corner,
with a clear central gathering space for multiplayer. No reference photos imported.
Re-running rebuilds TB_Lobby only; hand edits to that generated level are replaced.
"""
import unreal
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist('/Game/Maps/TB_Lobby'):
 assert L.load_level('/Game/Maps/TB_Lobby')
 for actor in A.get_all_level_actors():
  if isinstance(actor,(unreal.StaticMeshActor,unreal.Light,unreal.CameraActor,unreal.PlayerStart,unreal.TextRenderActor,unreal.PostProcessVolume,unreal.SkyAtmosphere)):A.destroy_actor(actor)
else:
 assert L.new_level('/Game/Maps/TB_Lobby')
W=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
W.get_world_settings().set_editor_property('default_game_mode',unreal.load_class(None,'/Script/ToonStory.TBGameMode'))
def spawn(cls,p,rot=0):
 return A.spawn_actor_from_class(cls,unreal.Vector(*p),unreal.Rotator(yaw=rot))
def material(name,color):
 path='/Game/Environment/Lobby/MI_'+name
 m=unreal.load_asset(path)
 if not m:
  m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('MI_'+name,'/Game/Environment/Lobby',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
  unreal.MaterialEditingLibrary.set_material_instance_parent(m,unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial'))
 if unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(m,'Color') != unreal.LinearColor(*color):
  unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(m,'Color',unreal.LinearColor(*color))
  unreal.EditorAssetLibrary.save_loaded_asset(m)
 return m
cream=material('Cream',(.76,.65,.46,1));sage=material('Sage',(.16,.29,.23,1));wood=material('Oak',(.34,.17,.065,1));coral=material('Coral',(.64,.17,.09,1));gold=material('Gold',(.83,.49,.12,1))
def cube(name,p,size,mat):
 a=spawn(unreal.StaticMeshActor,p);a.set_actor_label(name)
 a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
 a.static_mesh_component.set_material(0,mat);a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
 return a
def prop(name,x,y,z,width,yaw=0,collision=True):
 m=unreal.load_asset('/Game/ToyWorld/Meshes/'+name)
 if not m: raise RuntimeError(name)
 b=m.get_bounding_box();s=width/max(b.max.x-b.min.x,b.max.y-b.min.y)
 a=spawn(unreal.StaticMeshActor,(x,y,z-b.min.z*s),yaw);a.set_actor_label('Club_'+name)
 a.static_mesh_component.set_static_mesh(m);a.set_actor_scale3d(unreal.Vector(s,s,s))
 if not collision:a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
 return a
def flat_prop(name,x,y,z,length,yaw=0):
 # Long upright source meshes (pens/books) must be sized along all three axes.
 m=unreal.load_asset('/Game/ToyWorld/Meshes/'+name)
 b=m.get_bounding_box();scale=length/max(b.max.x-b.min.x,b.max.y-b.min.y,b.max.z-b.min.z)
 a=spawn(unreal.StaticMeshActor,(x,y,z),yaw)
 a.static_mesh_component.set_static_mesh(m)
 a.set_actor_scale3d(unreal.Vector(scale,scale,scale))
 a.set_actor_rotation(unreal.Rotator(yaw=yaw,roll=90),False)
 center,extent=a.get_actor_bounds(False)
 a.set_actor_location(a.get_actor_location()+unreal.Vector(x-center.x,y-center.y,z-center.z+extent.z),False,False)
 a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
 return a
# Timber parquet with actual normal/roughness textures at the source mesh's scale.
floor=unreal.load_asset('/Game/ToyWorld/Meshes/SM_FloorDetailed01')
for x in range(4):
 for y in range(3):
  a=spawn(unreal.StaticMeshActor,(-600+x*300,-450+y*300,0));a.static_mesh_component.set_static_mesh(floor);a.set_actor_label('Parquet')
plaster=unreal.load_asset('/Game/ToyWorld/Materials/MI_Wallpaper02')
timber=unreal.load_asset('/Game/ToyWorld/Materials/MI_WoodenCeiling01')
cube('Back wall',(0,460,190),(1220,20,380),cream)
cube('Left wall',(-610,0,190),(20,900,380),cream)
# Right wall is divided around a genuine window opening.
cube('Window wall sill',(610,0,50),(20,900,100),cream)
cube('Window wall lintel',(610,0,355),(20,900,50),cream)
for y in (-350,350):cube('Window wall side',(610,y,215),(20,200,230),cream)
cube('Window wall pier',(610,0,215),(20,24,230),cream)
boundary=cube('Front boundary',(0,-460,190),(1220,20,380),cream)
boundary.set_actor_hidden_in_game(True);boundary.static_mesh_component.set_cast_shadow(False)
boundary.static_mesh_component.set_collision_profile_name('InvisibleWall')

cube('Ceiling',(0,0,390),(1220,920,20),cream)
for x in (-592,592):cube('Skirting',(x,0,10),(6,900,20),cream)
cube('Back skirting',(0,446,10),(1200,8,20),cream)
# Subtle wallpapered rear wall; the large painted placard and paper bunting are removed.
for i in range(4):
 a=spawn(unreal.StaticMeshActor,(-300+i*300,449,0),180)
 a.static_mesh_component.set_static_mesh(unreal.load_asset('/Game/ToyWorld/Meshes/SM_WallPaper02'))
 a.set_actor_scale3d(unreal.Vector(1,1,1.25));a.set_actor_label('Textured wallpaper')
for y in (-135,135):
 prop('SM_Window01',600,y,100,180,180)
 for side in (-1,1):
  a=spawn(unreal.StaticMeshActor,(570,y+side*45,50),90)
  a.static_mesh_component.set_static_mesh(unreal.load_asset('/Game/ToyWorld/Meshes/SM_Curtain01'))
  a.set_actor_scale3d(unreal.Vector(-side*.45,.9,.99));a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
 a=spawn(unreal.StaticMeshActor,(570,y,50),90);a.static_mesh_component.set_static_mesh(unreal.load_asset('/Game/ToyWorld/Meshes/SM_CurtainRod'));a.set_actor_scale3d(unreal.Vector(.86,1,1));a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
# Sofa group at the far-left; shelves/reading chair on the opposite side.
prop('SM_CarpetTriPattern',-260,170,0,430,90,False)
prop('SM_Sofa_01',-290,340,0,300,-90)
table=prop('SM_Table_01',-280,145,0,145)
center,extent=table.get_actor_bounds(False)
prop('SM_BooksSet03',-310,145,center.z+extent.z,40,collision=False)
prop('SM_Vase',-245,145,center.z+extent.z,18,collision=False)
prop('SM_FloorLamp',-505,330,0,60)
prop('SM_Plants',-510,-250,0,85)
prop('SM_Plants',520,320,0,90)
prop('SM_WoodChair01',430,70,0,65,155)
prop('SM_TableLamp01',340,400,282.5,30,collision=False)
# Narrow full-height shelving, with books and a few toys rather than oversized blocks.
for z in (12,75,145,215,280):cube('Timber bookshelf',(240,403,z),(270,70,5),timber)
for x in (100,380):cube('Bookshelf upright',(x,403,146),(6,70,292),timber)
cube('Bookshelf back',(240,438,146),(280,5,292),timber)
for i,(z,n) in enumerate(((17,'SM_BooksSet04'),(80,'SM_BooksSet03'),(150,'SM_BooksSet04'),(220,'SM_BooksSet03'))):
 prop(n,175 if i%2 else 280,400,z,85,collision=False)
prop('SM_Train_01',300,400,220,65,collision=False)
prop('SM_Toy',160,400,80,45,collision=False)
prop('SM_ToyCar_01',300,400,150,50,collision=False)
# Lived-in playroom corners. The central arrival lane stays clear.
prop('SM_Sofa_02',-475,-5,0,185,0)
prop('SM_Pillow_02',-490,-10,53,38,15,False)
activity=prop('SM_KidsTable',370,-160,0,150,12)
activity_center,activity_extent=activity.get_actor_bounds(False)
activity_top=activity_center.z+activity_extent.z
prop('SM_WoodChair02',495,-185,0,55,180)
prop('SM_WoodChair01',340,-285,0,52,90)
for name,x,y,width,yaw in (
 ('SM_Notebook',395,-240,35,22),
 ('SM_standForPens',450,-215,15,-10),
 ('SM_pen_on',375,-215,18,35),
 ('SM_PuzzleCube',425,-275,16,12),
 ('SM_ToyCar_01',365,-270,23,-25)):
 if name in ('SM_Notebook','SM_pen_on'):flat_prop(name,x-35,y+80,activity_top,width,yaw)
 else:prop(name,x-35,y+80,activity_top,width,yaw,False)
storage=prop('SM_Drawer_01',-465,-185,0,120,0)
storage_center,storage_extent=storage.get_actor_bounds(False)
storage_top=storage_center.z+storage_extent.z
prop('SM_DollHouse01',-465,-185,storage_top,85,0,False)
prop('SM_TeddyBear_01',-510,-180,storage_top,25,25,False)
prop('SM_Box_empty',-295,-285,0,80,-12)
prop('SM_Box_closed',475,290,0,60,8)
prop('SM_Guitar',-550,185,0,46,-15,False)
prop('SM_Carpet08',305,-155,.3,300,90,False)
# Coffee table and reading corner details are grounded on their supporting surfaces.
for name,x,y,width,yaw in (
 ('SM_CoffeeCup_01',-290,115,12,0),
 ('SM_Remote_Control_01',-250,125,22,20),
 ('SM_Gamepad02',-320,165,25,-12)):
 prop(name,x,y,center.z+extent.z,width,yaw,False)
for name,x,y,z,width,yaw in (
 ('SM_TeddyBear_01',310,400,17,40,0),
 ('SM_PuzzleCube',145,400,150,24,10),
 ('SM_Plastic_Truck_01',175,400,220,55,-10),
 ('SM_BooksSet03',290,400,80,65,0),
 ('SM_PhotoFrame01',180,400,282.5,32,90),
 ('SM_Book03',465,300,61,40,15)):
 prop(name,x,y,z,width,yaw,False)
# Hand-placed play trails: train cars, toy vehicles, soldiers, puzzles and plush toys.
# Small decorations do not snag the lobby characters' movement capsule.
floor_toys=[
 ('SM_Train_01',260,-85,42,15),
 ('SM_Train_02',295,-95,35,15),
 ('SM_Train_03',330,-105,35,15),
 ('SM_ToyCar_01',220,-205,28,-35),
 ('SM_Tractor_01',310,-345,38,32),
 ('SM_Plastic_Truck_01',150,-330,40,-15),
 ('SM_Plastic_Truck_02',-250,-135,38,65),
 ('SM_Tractor_02',-325,-190,34,-40),
 ('SM_TeddyBear_01',-230,-310,46,25),
 ('SM_Toy',-355,-260,38,-20),
 ('SM_BasketBall',80,-290,26,0),
 ('SM_PuzzleCube',-145,-220,20,18),
 ('SM_Puzzle_01',290,-180,22,15),
 ('SM_Puzzle_02',320,-190,20,-30),
 ('SM_Puzzle_03',270,-215,23,75),
 ('SM_Puzzle_04',325,-235,18,-15),
 ('SM_Puzzle_05',345,-165,20,35),
 ('SM_soldier_toy01',-240,5,18,30),
 ('SM_Soldier_1',-275,-25,19,-15),
 ('SM_Soldier_2',-300,20,18,60),
 ('SM_ToyCar_01',-95,-365,25,110),
 ('SM_TeddyBear_01',485,175,38,-65),
 ('SM_Book06',-390,-140,35,20),
 ('SM_Notebook',365,-110,32,-15),
 ('SM_ToyCar_01',-380,85,24,140),
 ('SM_PuzzleCube',185,260,19,-20),
]
for name,x,y,width,yaw in floor_toys:
 if name in ('SM_Book06','SM_Notebook'):flat_prop(name,x,y,.2,width,yaw)
 else:prop(name,x,y,.2,width,yaw,False)
# Art above the seating area, modest club plaque beside the shelving.
prop('SM_Picture_01',-260,431,205,95,90,False)
cube('Small club plaque',(35,438,240),(100,5,45),wood)
a=spawn(unreal.TextRenderActor,(35,434,240),-90);a.text_render.set_text('TOON STORY');a.text_render.set_world_size(12);a.text_render.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER);a.text_render.set_text_render_color(unreal.Color(230,220,195))
for i in range(8):spawn(unreal.PlayerStart,(-210+(i%4)*140,-180+(i//4)*125,70),-90)
sun=spawn(unreal.DirectionalLight,(0,0,600));sun.set_actor_rotation(unreal.Rotator(pitch=-28,yaw=155),False);sun.light_component.set_intensity(3);sun.light_component.set_editor_property('atmosphere_sun_light',True);sun.light_component.set_editor_property('light_source_angle',3)
sky=spawn(unreal.SkyLight,(0,0,400));sky.light_component.set_editor_property('real_time_capture',True);sky.light_component.set_intensity(.6)
spawn(unreal.SkyAtmosphere,(0,0,0))
# Broad daylight sources aligned with the windows, plus soft front fill for faces.
for y in (-135,135):
 a=spawn(unreal.RectLight,(570,y,225),180);a.light_component.set_intensity(100);a.light_component.set_editor_property('source_width',180);a.light_component.set_editor_property('source_height',190);a.light_component.set_editor_property('attenuation_radius',1300);a.light_component.set_light_color(unreal.LinearColor(.83,.9,1))
a=spawn(unreal.RectLight,(0,-430,280),90);a.light_component.set_intensity(70);a.light_component.set_editor_property('source_width',700);a.light_component.set_editor_property('source_height',150);a.light_component.set_editor_property('attenuation_radius',1400)
a=spawn(unreal.PointLight,(-505,330,150));a.light_component.set_intensity(12);a.light_component.set_editor_property('attenuation_radius',300);a.light_component.set_light_color(unreal.LinearColor(1,.69,.4))
cam=spawn(unreal.CameraActor,(0,-780,265));cam.set_actor_rotation(unreal.Rotator(pitch=-9,yaw=90),False);cam.tags=['TBLobbyCamera'];cam.camera_component.set_field_of_view(76)
pp=spawn(unreal.PostProcessVolume,(0,0,0));pp.set_editor_property('unbound',True)
settings=pp.get_editor_property('settings')
for key,value in [('override_auto_exposure_min_brightness',True),('override_auto_exposure_max_brightness',True),('auto_exposure_min_brightness',1.2),('auto_exposure_max_brightness',1.2),('override_auto_exposure_bias',True),('auto_exposure_bias',0.0),('override_bloom_intensity',True),('bloom_intensity',.12)]:settings.set_editor_property(key,value)
pp.set_editor_property('settings',settings)
assert L.save_current_level()
unreal.log('CLUBHOUSE_SAVED')
