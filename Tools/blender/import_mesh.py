"""Editor side of the mesh pipeline (07_Technical R-TEC-06): imports SourceArt meshes as static meshes, headless (UE editor Python).

Run by Tools/blender/Mesh.ps1 through Tools/EditorPython.bat (UnrealEditor-Cmd -run=pythonscript), never by hand. Reads the environment:
    DT_MESH_FILES   the .fbx files to import, relative to the repo root, separated by ';'
    DT_MESH_DEST    the content folder they land in (default /Game/DT/Meshes)
Each file becomes one static mesh named as the file, its objects joined, replacing the asset in place so the asset path never changes.
Nothing but the mesh comes in: no material and no texture asset is made (a slot keeps its name and the default material; the game
assigns its own generated materials), no collision is built (rounds and hulls collide in the sim, never in the engine) and Nanite stays
off (04_Presentation §4: instanced and animated meshes never use Nanite).
Logs  import_mesh: <asset> tris=<n> size_uu=<x> <y> <z> min_uu=<x> <y> <z>  per mesh, then  import_mesh: DONE.
"""

import os

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
DEST = os.environ.get("DT_MESH_DEST", "/Game/DT/Meshes")
EAL = unreal.EditorAssetLibrary


def log(msg):
    unreal.log(f"import_mesh: {msg}")


def options():
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("generate_lightmap_u_vs", False)
    data.set_editor_property("build_nanite", False)
    data.set_editor_property("remove_degenerates", False)
    return ui


def import_one(relative):
    source = os.path.join(ROOT, relative)
    if not os.path.isfile(source):
        raise RuntimeError(f"{relative} does not exist")
    name = os.path.splitext(os.path.basename(source))[0]
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", options())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    path = f"{DEST}/{name}"
    mesh = EAL.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError(f"{relative} did not import as a static mesh at {path}")
    EAL.save_loaded_asset(mesh, only_if_is_dirty=False)
    box = mesh.get_bounding_box()
    size = box.max - box.min
    tris = mesh.get_num_triangles(0)
    if tris <= 0:
        raise RuntimeError(f"{path} has no triangle")
    log(f"{path} tris={tris} size_uu={size.x:.1f} {size.y:.1f} {size.z:.1f} min_uu={box.min.x:.1f} {box.min.y:.1f} {box.min.z:.1f}")


def main():
    files = [part.strip() for part in os.environ.get("DT_MESH_FILES", "").split(";") if part.strip()]
    if not files:
        raise RuntimeError("DT_MESH_FILES names no file")
    if not EAL.does_directory_exist(DEST):
        EAL.make_directory(DEST)
    for relative in files:
        import_one(relative)
    log("DONE")


try:
    main()
except Exception as exc:  # surface as a log error so Tools/EditorPython.bat fails
    unreal.log_error(f"import_mesh: FAILED: {exc}")
    raise
