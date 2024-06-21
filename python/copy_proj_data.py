import shutil, os, sys

src_proj_path = shutil.which("proj")
assert not src_proj_path is None
src_proj_path = os.path.join(os.path.dirname(os.path.dirname(src_proj_path)), "share", "proj")
assert len(os.listdir(src_proj_path)) > 0

dest_proj_path = os.path.join(sys.argv[1], "proj_data")
if os.path.isdir(dest_proj_path):
    shutil.rmtree(dest_proj_path)

print(f"Copying from {src_proj_path} to {dest_proj_path}")
shutil.copytree(src_proj_path, dest_proj_path)