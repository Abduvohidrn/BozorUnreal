"""Create six native Blender cloth materials, bake their colors and render previews."""
import bpy
import json
import math
import numpy as np
from pathlib import Path
from mathutils import Vector

ROOT=Path(r"D:/Unreal Projects/BozorUnreal")
OUT=ROOT/"Assets/Romol/Variants"
bpy.ops.wm.open_mainfile(filepath=str(ROOT/"Saved/Romol/Romol.blend"))
scene=bpy.context.scene
rom=bpy.data.objects["Romol"]
camera=scene.camera
scene.cycles.samples=24
scene.render.resolution_x=scene.render.resolution_y=600
scene.render.resolution_percentage=100
target=Vector((0,1.5,162.0))
camera.location=target+Vector((.68,1,.22)).normalized()*75
camera.rotation_euler=(target-camera.location).to_track_quat("-Z","Y").to_euler()
camera.data.ortho_scale=45
variants=[
    ("01_Kok_Moychechak","KO'K  /  MOYCHECHAK","#102c58","#f1e7d2","#dca83c","#447d84",12,9,.12),
    ("02_Zumrad_Atirgul","ZUMRAD  /  ATIRGUL","#124c41","#dca4a5","#f0c795","#65856a",10,7,.065),
    ("03_Qora_Oltin","QORA  /  OLTIN GUL","#151a21","#c49d53","#f6e8bc","#665331",16,8,.11),
    ("04_Sutrang_Lola","SUTRANG  /  LOLA","#eee5d1","#a63e54","#d8a762","#6b825c",12,3,.14),
    ("05_Binafsha_Yulduz","BINAFSHA  /  YULDUZGUL","#453353","#d3b7dc","#f0d8a0","#878ca0",14,6,.13),
    ("06_Bordo_Chinnigul","BORDO  /  CHINNIGUL","#641f35","#e9bfac","#e6b64f","#7c926a",11,12,.055),
]
def color(value):
    rgb=[int(value[i:i+2],16)/255 for i in (1,3,5)]
    return tuple(v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in rgb)+(1,)

def make_pattern(spec):
    name,label,bg,petal,heart,leaf,density,petals,lobe=spec
    mat=bpy.data.materials.new("Pattern_"+name)
    mat.use_fake_user=True
    ns=mat.node_tree.nodes
    ns.clear()
    links=mat.node_tree.links
    def mathnode(op,a,b=0):
        n=ns.new("ShaderNodeMath")
        n.operation=op
        for slot,value in zip(n.inputs,(a,b)):
            if isinstance(value,(int,float)):
                slot.default_value=value
            else:
                links.new(value,slot)
        return n.outputs[0]
    def mix(base,top,mask):
        n=ns.new("ShaderNodeMixRGB")
        n.blend_type="MIX"
        for slot,value in ((n.inputs[0],mask),(n.inputs[1],base),(n.inputs[2],top)):
            if isinstance(value,tuple):
                slot.default_value=value
            else:
                links.new(value,slot)
        return n.outputs[0]
    uv=ns.new("ShaderNodeTexCoord")
    sep=ns.new("ShaderNodeSeparateXYZ")
    links.new(uv.outputs["UV"],sep.inputs[0])
    u,v=sep.outputs["X"],sep.outputs["Y"]
    row=mathnode("FLOOR",mathnode("MULTIPLY",v,density))
    stagger=mathnode("MULTIPLY",mathnode("MODULO",row,2),.5)
    x=mathnode("SUBTRACT",mathnode("FRACT",mathnode("ADD",mathnode("MULTIPLY",u,density),stagger)),.5)
    y=mathnode("SUBTRACT",mathnode("FRACT",mathnode("MULTIPLY",v,density)),.5)
    radius=mathnode("SQRT",mathnode("ADD",mathnode("MULTIPLY",x,x),mathnode("MULTIPLY",y,y)))
    angle=mathnode("ARCTAN2",y,x)
    wave=mathnode("COSINE",mathnode("MULTIPLY",angle,petals))
    contour=mathnode("ADD",.23,mathnode("MULTIPLY",wave,lobe))
    bloom=mathnode("LESS_THAN",radius,contour)
    inner=mathnode("LESS_THAN",radius,mathnode("MULTIPLY",contour,.67))
    center=mathnode("LESS_THAN",radius,.063)
    # Paired pointed leaves alongside each blossom.
    diagonal=mathnode("ADD",x,y)
    transverse=mathnode("SUBTRACT",x,y)
    leaf_distance=mathnode("ADD",
        mathnode("MULTIPLY",mathnode("ABSOLUTE",mathnode("SUBTRACT",mathnode("ABSOLUTE",diagonal),.57)),3.8),
        mathnode("MULTIPLY",mathnode("ABSOLUTE",transverse),12))
    leaves=mathnode("LESS_THAN",leaf_distance,1)
    result=mix(color(bg),color(leaf),leaves)
    result=mix(result,color(petal),bloom)
    if petals in (7,12):
        ring=mathnode("MULTIPLY",inner,mathnode("GREATER_THAN",mathnode("SINE",mathnode("ADD",mathnode("MULTIPLY",radius,85),angle)),.2))
        result=mix(result,color(heart),ring)
    result=mix(result,color(heart),center)
    # Two matching woven borders fit the existing hem islands.
    border_v=mathnode("FRACT",mathnode("MULTIPLY",v,2))
    border=mathnode("LESS_THAN",border_v,.078)
    stripe=mathnode("MULTIPLY",border,mathnode("GREATER_THAN",mathnode("COSINE",mathnode("MULTIPLY",border_v,420)),.62))
    result=mix(result,color(bg),border)
    result=mix(result,color(heart),stripe)
    emission=ns.new("ShaderNodeEmission")
    links.new(result,emission.inputs["Color"])
    output=ns.new("ShaderNodeOutputMaterial")
    links.new(emission.outputs[0],output.inputs["Surface"])
    return mat

# A unit UV tile gives engine-independent base-color maps without lighting.
for ob in bpy.context.selected_objects:
    ob.select_set(False)
bpy.ops.mesh.primitive_plane_add(size=2,location=(0,0,-500))
tile=bpy.context.object
tile.name="PatternBakeTile"
scene.render.engine="CYCLES"
scene.cycles.samples=4
materials=[]
for spec in variants:
    name=spec[0]
    pattern=make_pattern(spec)
    tile.data.materials.clear()
    tile.data.materials.append(pattern)
    image=bpy.data.images.new("T_Romol_"+name,width=2048,height=2048)
    image.colorspace_settings.name="sRGB"
    node=pattern.node_tree.nodes.new("ShaderNodeTexImage")
    node.image=image
    pattern.node_tree.nodes.active=node
    bpy.context.view_layer.objects.active=tile
    tile.select_set(True)
    bpy.ops.object.bake(type="EMIT",margin=0,use_clear=True)
    image.filepath_raw=str(OUT/(image.name+".png"))
    image.file_format="PNG"
    image.save()
    image.pack()
    mat=rom.data.materials[0].copy()
    mat.name="M_Romol_"+name
    mat.use_fake_user=True
    mat.diffuse_color=color(spec[2])
    textures=[n for n in mat.node_tree.nodes if n.type=="TEX_IMAGE"]
    for tex in textures:
        tex.image=image
    mat.node_tree.nodes.active=textures[0]
    mat.asset_mark()
    materials.append(mat)
bpy.data.objects.remove(tile,do_unlink=True)
scene.cycles.samples=24

# Camera-facing labels belong to the preview scene.
font=bpy.data.curves.new("Variant label","FONT")
font.align_x="CENTER"
font.size=1.05
label=bpy.data.objects.new("Variant label",font)
scene.collection.objects.link(label)
label.rotation_euler=camera.rotation_euler
label.location=target+camera.rotation_euler.to_quaternion()@Vector((0,-20.2,0))
label_mat=bpy.data.materials.new("Preview label")
label_mat.node_tree.nodes.clear()
em=label_mat.node_tree.nodes.new("ShaderNodeEmission")
em.inputs["Color"].default_value=(.85,.80,.72,1)
output=label_mat.node_tree.nodes.new("ShaderNodeOutputMaterial")
label_mat.node_tree.links.new(em.outputs[0],output.inputs["Surface"])
font.materials.append(label_mat)

previews=[]
for spec,mat in zip(variants,materials):
    name,title=spec[:2]
    rom.data.materials[0]=mat
    font.body=title
    scene.render.filepath=str(OUT/(name+".png"))
    bpy.ops.render.render(write_still=True)
    previews.append(scene.render.filepath)
    for ob in bpy.context.selected_objects:
        ob.select_set(False)
    rom.select_set(True)
    bpy.context.view_layer.objects.active=rom
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(name+".blend")))

# Contact sheet assembled from the six verified Blender renders.
sheet=np.ones((1200,1800,4),dtype=np.float32)
for index,path in enumerate(previews):
    img=bpy.data.images.load(path,check_existing=False)
    pixels=np.empty(600*600*4,dtype=np.float32)
    img.pixels.foreach_get(pixels)
    x=(index%3)*600
    y=(1-index//3)*600
    sheet[y:y+600,x:x+600]=pixels.reshape(600,600,4)
    bpy.data.images.remove(img)
gallery=bpy.data.images.new("Romol variants",width=1800,height=1200)
gallery.pixels.foreach_set(sheet.ravel())
gallery.filepath_raw=str(OUT/"Romol_Variantlar.png")
gallery.file_format="PNG"
gallery.save()
manifest=[dict(name=s[0],label=s[1],blend=s[0]+".blend",base_color="T_Romol_"+s[0]+".png",material=m.name) for s,m in zip(variants,materials)]
(OUT/"variants.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")
print(json.dumps({"variants":len(materials),"textures":6,"resolution":2048,"gallery":gallery.filepath_raw}))
